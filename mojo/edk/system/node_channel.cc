// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/node_channel.h"

#include <cstring>
#include <limits>
#include <sstream>

#include "base/logging.h"
#include "mojo/edk/system/channel.h"

namespace mojo {
namespace edk {

namespace {

template <typename T>
T Align(T t) {
  const auto k = kChannelMessageAlignment;
  return t + (k - (t % k)) % k;
}

enum class MessageType : uint32_t {
  ACCEPT_CHILD,
  ACCEPT_PARENT,
  PORTS_MESSAGE,
  REQUEST_PORT_CONNECTION,
  CONNECT_TO_PORT,
  REQUEST_INTRODUCTION,
  INTRODUCE,
#if defined(OS_WIN)
  RELAY_PORTS_MESSAGE,
#endif
};

struct Header {
  MessageType type;
  uint32_t padding;
};

static_assert(sizeof(Header) % kChannelMessageAlignment == 0,
    "Invalid header size.");

struct AcceptChildData {
  ports::NodeName parent_name;
  ports::NodeName token;
};

struct AcceptParentData {
  ports::NodeName token;
  ports::NodeName child_name;
};

// This is followed by arbitrary payload data which is interpreted as a token
// string for port location.
struct RequestPortConnectionData {
  ports::PortName connector_port_name;
};

struct ConnectToPortData {
  ports::PortName connector_port_name;
  ports::PortName connectee_port_name;
};

// Used for both REQUEST_INTRODUCTION and INTRODUCE.
//
// For INTRODUCE the message must also include a platform handle the recipient
// can use to communicate with the named node. If said handle is omitted, the
// peer cannot be introduced.
struct IntroductionData {
  ports::NodeName name;
};

#if defined(OS_WIN)
// This struct is followed by the full payload of a message to be relayed.
struct RelayPortsMessageData {
  ports::NodeName destination;
};
#endif

template <typename DataType>
Channel::MessagePtr CreateMessage(MessageType type,
                                  size_t payload_size,
                                  size_t num_handles,
                                  DataType** out_data) {
  Channel::MessagePtr message(
      new Channel::Message(sizeof(Header) + payload_size, num_handles));
  Header* header = reinterpret_cast<Header*>(message->mutable_payload());
  header->type = type;
  header->padding = 0;
  *out_data = reinterpret_cast<DataType*>(&header[1]);
  return message;
};

template <typename DataType>
void GetMessagePayload(const void* bytes, DataType** out_data) {
  *out_data = reinterpret_cast<const DataType*>(
      static_cast<const char*>(bytes) + sizeof(Header));
}

}  // namespace

// static
scoped_refptr<NodeChannel> NodeChannel::Create(
    Delegate* delegate,
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new NodeChannel(delegate, std::move(platform_handle), io_task_runner);
}

// static
Channel::MessagePtr NodeChannel::CreatePortsMessage(size_t payload_size,
                                                    void** payload,
                                                    size_t num_handles) {
  return CreateMessage(MessageType::PORTS_MESSAGE, payload_size, num_handles,
                       payload);
}

// static
void NodeChannel::GetPortsMessageData(Channel::Message* message,
                                      void** data,
                                      size_t* num_data_bytes) {
  *data = reinterpret_cast<Header*>(message->mutable_payload()) + 1;
  *num_data_bytes = message->payload_size() - sizeof(Header);
}

void NodeChannel::Start() {
  base::AutoLock lock(channel_lock_);
  DCHECK(channel_);
  channel_->Start();
}

void NodeChannel::ShutDown() {
  base::AutoLock lock(channel_lock_);
  if (channel_) {
    channel_->ShutDown();
    channel_ = nullptr;
  }
}

void NodeChannel::SetRemoteProcessHandle(base::ProcessHandle process_handle) {
#if defined(OS_WIN)
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  base::AutoLock lock(remote_process_handle_lock_);
  remote_process_handle_ = process_handle;
#endif
}

void NodeChannel::SetRemoteNodeName(const ports::NodeName& name) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  remote_node_name_ = name;
}

void NodeChannel::AcceptChild(const ports::NodeName& parent_name,
                              const ports::NodeName& token) {
  AcceptChildData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_CHILD, sizeof(AcceptChildData), 0, &data);
  data->parent_name = parent_name;
  data->token = token;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::AcceptParent(const ports::NodeName& token,
                               const ports::NodeName& child_name) {
  AcceptParentData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::ACCEPT_PARENT, sizeof(AcceptParentData), 0, &data);
  data->token = token;
  data->child_name = child_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::PortsMessage(Channel::MessagePtr message) {
  WriteChannelMessage(std::move(message));
}

void NodeChannel::RequestPortConnection(
    const ports::PortName& connector_port_name,
    const std::string& token) {
  RequestPortConnectionData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::REQUEST_PORT_CONNECTION,
      sizeof(RequestPortConnectionData) + token.size(), 0, &data);
  data->connector_port_name = connector_port_name;
  memcpy(data + 1, token.data(), token.size());
  WriteChannelMessage(std::move(message));
}

void NodeChannel::ConnectToPort(const ports::PortName& connector_port_name,
                                const ports::PortName& connectee_port_name) {
  ConnectToPortData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::CONNECT_TO_PORT, sizeof(ConnectToPortData), 0, &data);
  data->connector_port_name = connector_port_name;
  data->connectee_port_name = connectee_port_name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::RequestIntroduction(const ports::NodeName& name) {
  IntroductionData* data;
  Channel::MessagePtr message = CreateMessage(
      MessageType::REQUEST_INTRODUCTION, sizeof(IntroductionData), 0, &data);
  data->name = name;
  WriteChannelMessage(std::move(message));
}

void NodeChannel::Introduce(const ports::NodeName& name,
                            ScopedPlatformHandle handle) {
  IntroductionData* data;
  ScopedPlatformHandleVectorPtr handles;
  if (handle.is_valid()) {
    handles.reset(new PlatformHandleVector(1));
    handles->at(0) = handle.release();
  }
  Channel::MessagePtr message = CreateMessage(
      MessageType::INTRODUCE, sizeof(IntroductionData), handles ? 1 : 0, &data);
  message->SetHandles(std::move(handles));
  data->name = name;
  WriteChannelMessage(std::move(message));
}

#if defined(OS_WIN)
void NodeChannel::RelayPortsMessage(const ports::NodeName& destination,
                                    Channel::MessagePtr message) {
  DCHECK(message->has_handles());

  // Note that this is only used on Windows, and on Windows all platform
  // handles are included in the message data. We blindly copy all the data
  // here and the relay node (the parent) will duplicate handles as needed.
  size_t num_bytes = sizeof(RelayPortsMessageData) + message->data_num_bytes();
  RelayPortsMessageData* data;
  Channel::MessagePtr relay_message = CreateMessage(
      MessageType::RELAY_PORTS_MESSAGE, num_bytes, 0, &data);
  data->destination = destination;
  memcpy(data + 1, message->data(), message->data_num_bytes());

  // When the handles are duplicated in the parent, the source handles will
  // be closed. If the parent never receives this message then these handles
  // will leak, but that means something else has probably broken and the
  // sending process won't likely be around much longer.
  ScopedPlatformHandleVectorPtr handles = message->TakeHandles();
  handles->clear();

  WriteChannelMessage(std::move(relay_message));
}
#endif

NodeChannel::NodeChannel(Delegate* delegate,
                         ScopedPlatformHandle platform_handle,
                         scoped_refptr<base::TaskRunner> io_task_runner)
    : delegate_(delegate),
      io_task_runner_(io_task_runner),
      channel_(
          Channel::Create(this, std::move(platform_handle), io_task_runner_)) {
}

NodeChannel::~NodeChannel() {
  ShutDown();
}

void NodeChannel::OnChannelMessage(const void* payload,
                                   size_t payload_size,
                                   ScopedPlatformHandleVectorPtr handles) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  const Header* header = static_cast<const Header*>(payload);
  switch (header->type) {
    case MessageType::ACCEPT_CHILD: {
      const AcceptChildData* data;
      GetMessagePayload(payload, &data);
      delegate_->OnAcceptChild(remote_node_name_, data->parent_name,
                               data->token);
      break;
    }

    case MessageType::ACCEPT_PARENT: {
      const AcceptParentData* data;
      GetMessagePayload(payload, &data);
      delegate_->OnAcceptParent(remote_node_name_, data->token,
                                data->child_name);
      break;
    }

    case MessageType::PORTS_MESSAGE: {
      size_t num_handles = handles ? handles->size() : 0;
      Channel::MessagePtr message(
          new Channel::Message(payload_size, num_handles));
      message->SetHandles(std::move(handles));
      memcpy(message->mutable_payload(), payload, payload_size);
      delegate_->OnPortsMessage(std::move(message));
      break;
    }

    case MessageType::REQUEST_PORT_CONNECTION: {
      const RequestPortConnectionData* data;
      GetMessagePayload(payload, &data);

      const char* token_data = reinterpret_cast<const char*>(data + 1);
      const size_t token_size = payload_size - sizeof(*data) - sizeof(Header);
      std::string token(token_data, token_size);

      delegate_->OnRequestPortConnection(remote_node_name_,
                                         data->connector_port_name, token);
      break;
    }

    case MessageType::CONNECT_TO_PORT: {
      const ConnectToPortData* data;
      GetMessagePayload(payload, &data);
      delegate_->OnConnectToPort(remote_node_name_, data->connector_port_name,
                                 data->connectee_port_name);
      break;
    }

    case MessageType::REQUEST_INTRODUCTION: {
      const IntroductionData* data;
      GetMessagePayload(payload, &data);
      delegate_->OnRequestIntroduction(remote_node_name_, data->name);
      break;
    }

    case MessageType::INTRODUCE: {
      const IntroductionData* data;
      GetMessagePayload(payload, &data);
      ScopedPlatformHandle handle;
      if (handles && !handles->empty()) {
        handle = ScopedPlatformHandle(handles->at(0));
        handles->clear();
      }
      delegate_->OnIntroduce(remote_node_name_, data->name, std::move(handle));
      break;
    }

#if defined(OS_WIN)
    case MessageType::RELAY_PORTS_MESSAGE: {
      base::ProcessHandle from_process;
      {
        base::AutoLock lock(remote_process_handle_lock_);
        from_process = remote_process_handle_;
      }
      const RelayPortsMessageData* data;
      GetMessagePayload(payload, &data);
      const void* message_start = data + 1;
      Channel::MessagePtr message = Channel::Message::Deserialize(
          message_start, payload_size - sizeof(Header) - sizeof(*data));
      if (!message) {
        DLOG(ERROR) << "Dropping invalid relay message.";
        break;
      }
      delegate_->OnRelayPortsMessage(remote_node_name_, from_process,
                                     data->destination, std::move(message));
      break;
    }
#endif

    default:
      DLOG(ERROR) << "Received unknown message type "
                  << static_cast<uint32_t>(header->type) << " from node "
                  << remote_node_name_;
      delegate_->OnChannelError(remote_node_name_);
      break;
  }
}

void NodeChannel::OnChannelError() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());

  ShutDown();
  // |OnChannelError()| may cause |this| to be destroyed, but still need access
  // to the name name after that destruction. So may a copy of
  // |remote_node_name_| so it can be used if |this| becomes destroyed.
  ports::NodeName node_name = remote_node_name_;
  delegate_->OnChannelError(node_name);
}

void NodeChannel::WriteChannelMessage(Channel::MessagePtr message) {
#if defined(OS_WIN)
  // Map handles to the destination process. Note: only messages from the parent
  // node should contain handles on Windows. If a child node needs to send
  // handles, it should do so via RelayPortsMessage, which stashes the handles
  // in the message in such a way that they go undetected here.

  if (message->has_handles()) {
    base::ProcessHandle remote_process_handle;
    {
      base::AutoLock lock(remote_process_handle_lock_);
      remote_process_handle = remote_process_handle_;
    }

    if (remote_process_handle == base::kNullProcessHandle) {
      DLOG(ERROR) << "Sending a message with handles as a non-parent. "
                  << "This is most likely broken.";
    } else {
      for (size_t i = 0; i < message->num_handles(); ++i) {
        BOOL result = DuplicateHandle(
            base::GetCurrentProcessHandle(), message->handles()[i].handle,
            remote_process_handle,
            reinterpret_cast<HANDLE*>(message->handles() + i), 0, FALSE,
            DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
        DCHECK(result);
      }
    }
  }
#endif

  base::AutoLock lock(channel_lock_);
  if (!channel_)
    DLOG(ERROR) << "Dropping message on closed channel.";
  else
    channel_->Write(std::move(message));
}

}  // namespace edk
}  // namespace mojo
