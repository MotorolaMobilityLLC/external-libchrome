// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel_endpoint.h"

#include "base/logging.h"
#include "mojo/edk/system/channel.h"
#include "mojo/edk/system/message_pipe.h"
#include "mojo/edk/system/transport_data.h"

namespace mojo {
namespace system {

ChannelEndpoint::ChannelEndpoint(MessagePipe* message_pipe, unsigned port)
    : state_(STATE_NORMAL),
      message_pipe_(message_pipe),
      port_(port),
      channel_(),
      local_id_(MessageInTransit::kInvalidEndpointId),
      remote_id_(MessageInTransit::kInvalidEndpointId) {
  DCHECK(message_pipe_.get());
  DCHECK(port_ == 0 || port_ == 1);
}

void ChannelEndpoint::TakeMessages(MessageInTransitQueue* message_queue) {
  DCHECK(paused_message_queue_.IsEmpty());
  paused_message_queue_.Swap(message_queue);
}

bool ChannelEndpoint::EnqueueMessage(scoped_ptr<MessageInTransit> message) {
  DCHECK(message);

  base::AutoLock locker(lock_);

  if (!channel_ || remote_id_ == MessageInTransit::kInvalidEndpointId) {
    // We may reach here if we haven't been attached or run yet.
    // TODO(vtl): We may also reach here if the channel is shut down early for
    // some reason (with live message pipes on it). We can't check |state_| yet,
    // until it's protected under lock, but in this case we should return false
    // (and not enqueue any messages).
    paused_message_queue_.AddMessage(message.Pass());
    return true;
  }

  // TODO(vtl): Currently, this only works in the "running" case.
  DCHECK_NE(remote_id_, MessageInTransit::kInvalidEndpointId);

  return WriteMessageNoLock(message.Pass());
}

void ChannelEndpoint::DetachFromMessagePipe() {
  // TODO(vtl): Once |message_pipe_| is under |lock_|, we should null it out
  // here. For now, get the channel to do so for us.

  {
    base::AutoLock locker(lock_);
    DCHECK(message_pipe_.get());
    message_pipe_ = nullptr;

    if (!channel_)
      return;
    DCHECK_NE(local_id_, MessageInTransit::kInvalidEndpointId);
    // TODO(vtl): Once we combine "run" into "attach", |remote_id_| should valid
    // here as well.
    channel_->DetachEndpoint(this, local_id_, remote_id_);
  }
}

void ChannelEndpoint::AttachToChannel(Channel* channel,
                                      MessageInTransit::EndpointId local_id) {
  DCHECK(channel);
  DCHECK_NE(local_id, MessageInTransit::kInvalidEndpointId);

  base::AutoLock locker(lock_);
  DCHECK(!channel_);
  DCHECK_EQ(local_id_, MessageInTransit::kInvalidEndpointId);
  channel_ = channel;
  local_id_ = local_id;
}

void ChannelEndpoint::Run(MessageInTransit::EndpointId remote_id) {
  DCHECK_NE(remote_id, MessageInTransit::kInvalidEndpointId);

  base::AutoLock locker(lock_);
  DCHECK(channel_);
  DCHECK_EQ(remote_id_, MessageInTransit::kInvalidEndpointId);
  remote_id_ = remote_id;

  while (!paused_message_queue_.IsEmpty()) {
    LOG_IF(WARNING, !WriteMessageNoLock(paused_message_queue_.GetMessage()))
        << "Failed to write enqueue message to channel";
  }
}

bool ChannelEndpoint::OnReadMessage(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  scoped_ptr<MessageInTransit> message(new MessageInTransit(message_view));
  scoped_refptr<MessagePipe> message_pipe;
  unsigned port;
  {
    base::AutoLock locker(lock_);
    DCHECK(channel_);
    if (!message_pipe_.get()) {
      // This isn't a failure per se. (It just means that, e.g., the other end
      // of the message point closed first.)
      return true;
    }

    if (message_view.transport_data_buffer_size() > 0) {
      DCHECK(message_view.transport_data_buffer());
      message->SetDispatchers(TransportData::DeserializeDispatchers(
          message_view.transport_data_buffer(),
          message_view.transport_data_buffer_size(),
          platform_handles.Pass(),
          channel_));
    }

    // Take a ref, and call |EnqueueMessage()| outside the lock.
    message_pipe = message_pipe_;
    port = port_;
  }

  MojoResult result = message_pipe->EnqueueMessage(
      MessagePipe::GetPeerPort(port), message.Pass());
  return (result == MOJO_RESULT_OK);
}

void ChannelEndpoint::OnDisconnect() {
  scoped_refptr<MessagePipe> message_pipe;
  unsigned port;
  {
    base::AutoLock locker(lock_);
    if (!message_pipe_.get())
      return;

    // Take a ref, and call |Close()| outside the lock.
    message_pipe = message_pipe_;
    port = port_;
  }
  message_pipe->Close(port);
}

void ChannelEndpoint::DetachFromChannel() {
  base::AutoLock locker(lock_);
  DCHECK(channel_);
  DCHECK_NE(local_id_, MessageInTransit::kInvalidEndpointId);
  // TODO(vtl): Once we combine "run" into "attach", |remote_id_| should valid
  // here as well.
  channel_ = nullptr;
  local_id_ = MessageInTransit::kInvalidEndpointId;
  remote_id_ = MessageInTransit::kInvalidEndpointId;
}

ChannelEndpoint::~ChannelEndpoint() {
  DCHECK(!message_pipe_.get());
  DCHECK(!channel_);
  DCHECK_EQ(local_id_, MessageInTransit::kInvalidEndpointId);
  DCHECK_EQ(remote_id_, MessageInTransit::kInvalidEndpointId);
}

bool ChannelEndpoint::WriteMessageNoLock(scoped_ptr<MessageInTransit> message) {
  DCHECK(message);

  lock_.AssertAcquired();

  DCHECK(channel_);
  DCHECK_NE(local_id_, MessageInTransit::kInvalidEndpointId);
  DCHECK_NE(remote_id_, MessageInTransit::kInvalidEndpointId);

  message->SerializeAndCloseDispatchers(channel_);
  message->set_source_id(local_id_);
  message->set_destination_id(remote_id_);
  return channel_->WriteMessage(message.Pass());
}

}  // namespace system
}  // namespace mojo
