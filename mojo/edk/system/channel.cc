// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"

#include <algorithm>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "mojo/edk/embedder/platform_handle_vector.h"
#include "mojo/edk/system/message_pipe_endpoint.h"
#include "mojo/edk/system/transport_data.h"

namespace mojo {
namespace system {

Channel::Channel(embedder::PlatformSupport* platform_support)
    : platform_support_(platform_support),
      is_running_(false),
      is_shutting_down_(false),
      next_local_id_(ChannelEndpointId::GetBootstrap()) {
}

bool Channel::Init(scoped_ptr<RawChannel> raw_channel) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());
  DCHECK(raw_channel);

  // No need to take |lock_|, since this must be called before this object
  // becomes thread-safe.
  DCHECK(!is_running_);
  raw_channel_ = raw_channel.Pass();

  if (!raw_channel_->Init(this)) {
    raw_channel_.reset();
    return false;
  }

  is_running_ = true;
  return true;
}

void Channel::Shutdown() {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  IdToEndpointMap to_destroy;
  {
    base::AutoLock locker(lock_);
    if (!is_running_)
      return;

    // Note: Don't reset |raw_channel_|, in case we're being called from within
    // |OnReadMessage()| or |OnError()|.
    raw_channel_->Shutdown();
    is_running_ = false;

    // We need to deal with it outside the lock.
    std::swap(to_destroy, local_id_to_endpoint_map_);
  }

  size_t num_live = 0;
  size_t num_zombies = 0;
  for (IdToEndpointMap::iterator it = to_destroy.begin();
       it != to_destroy.end();
       ++it) {
    if (it->second.get()) {
      num_live++;
      it->second->OnDisconnect();
      it->second->DetachFromChannel();
    } else {
      num_zombies++;
    }
  }
  DVLOG_IF(2, num_live || num_zombies) << "Shut down Channel with " << num_live
                                       << " live endpoints and " << num_zombies
                                       << " zombies";
}

void Channel::WillShutdownSoon() {
  base::AutoLock locker(lock_);
  is_shutting_down_ = true;
}

// Note: |endpoint| being a |scoped_refptr| makes this function safe, since it
// keeps the endpoint alive even after the lock is released. Otherwise, there's
// the temptation to simply pass the result of |new ChannelEndpoint(...)|
// directly to this function, which wouldn't be sufficient for safety.
ChannelEndpointId Channel::AttachEndpoint(
    scoped_refptr<ChannelEndpoint> endpoint) {
  DCHECK(endpoint.get());

  ChannelEndpointId local_id;
  {
    base::AutoLock locker(lock_);

    DLOG_IF(WARNING, is_shutting_down_)
        << "AttachEndpoint() while shutting down";

    while (!next_local_id_.is_valid() ||
           local_id_to_endpoint_map_.find(next_local_id_) !=
               local_id_to_endpoint_map_.end())
      next_local_id_.value++;

    local_id = next_local_id_;
    next_local_id_.value++;
    local_id_to_endpoint_map_[local_id] = endpoint;
  }

  endpoint->AttachToChannel(this, local_id);
  return local_id;
}

// TODO(vtl): This function is currently slightly absurd, but we'll eventually
// get rid of it and merge it with |AttachEndpoint()|.
void Channel::RunEndpoint(scoped_refptr<ChannelEndpoint> endpoint,
                          ChannelEndpointId remote_id) {
  {
    base::AutoLock locker(lock_);

    DLOG_IF(WARNING, is_shutting_down_)
        << "RunMessagePipeEndpoint() while shutting down";
  }

  // TODO(vtl): FIXME -- We need to handle the case that message pipe is already
  // running when we're here due to |kSubtypeChannelRunMessagePipeEndpoint|).
  endpoint->Run(remote_id);
}

void Channel::RunRemoteMessagePipeEndpoint(ChannelEndpointId local_id,
                                           ChannelEndpointId remote_id) {
#if DCHECK_IS_ON
  {
    base::AutoLock locker(lock_);
    DCHECK(local_id_to_endpoint_map_.find(local_id) !=
           local_id_to_endpoint_map_.end());
  }
#endif

  if (!SendControlMessage(
          MessageInTransit::kSubtypeChannelRunMessagePipeEndpoint,
          local_id,
          remote_id)) {
    HandleLocalError(base::StringPrintf(
        "Failed to send message to run remote message pipe endpoint (local ID "
        "%u, remote ID %u)",
        static_cast<unsigned>(local_id.value),
        static_cast<unsigned>(remote_id.value)));
  }
}

bool Channel::WriteMessage(scoped_ptr<MessageInTransit> message) {
  base::AutoLock locker(lock_);
  if (!is_running_) {
    // TODO(vtl): I think this is probably not an error condition, but I should
    // think about it (and the shutdown sequence) more carefully.
    LOG(WARNING) << "WriteMessage() after shutdown";
    return false;
  }

  DLOG_IF(WARNING, is_shutting_down_) << "WriteMessage() while shutting down";
  return raw_channel_->WriteMessage(message.Pass());
}

bool Channel::IsWriteBufferEmpty() {
  base::AutoLock locker(lock_);
  if (!is_running_)
    return true;
  return raw_channel_->IsWriteBufferEmpty();
}

void Channel::DetachEndpoint(ChannelEndpoint* endpoint,
                             ChannelEndpointId local_id,
                             ChannelEndpointId remote_id) {
  DCHECK(endpoint);
  DCHECK(local_id.is_valid());

  if (!remote_id.is_valid())
    return;  // Nothing to do.

  {
    base::AutoLock locker_(lock_);
    if (!is_running_)
      return;

    IdToEndpointMap::iterator it = local_id_to_endpoint_map_.find(local_id);
    // We detach immediately if we receive a remove message, so it's possible
    // that the local ID is no longer in |local_id_to_endpoint_map_|, or even
    // that it's since been reused for another endpoint. In both cases, there's
    // nothing more to do.
    if (it == local_id_to_endpoint_map_.end() || it->second.get() != endpoint)
      return;

    DCHECK(it->second.get());
    it->second = nullptr;

    // Send a remove message outside the lock.
  }

  if (!SendControlMessage(
          MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpoint,
          local_id,
          remote_id)) {
    HandleLocalError(base::StringPrintf(
        "Failed to send message to remove remote message pipe endpoint (local "
        "ID %u, remote ID %u)",
        static_cast<unsigned>(local_id.value),
        static_cast<unsigned>(remote_id.value)));
  }
}

size_t Channel::GetSerializedPlatformHandleSize() const {
  return raw_channel_->GetSerializedPlatformHandleSize();
}

Channel::~Channel() {
  // The channel should have been shut down first.
  DCHECK(!is_running_);
}

void Channel::OnReadMessage(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  switch (message_view.type()) {
    case MessageInTransit::kTypeMessagePipeEndpoint:
    case MessageInTransit::kTypeMessagePipe:
      OnReadMessageForDownstream(message_view, platform_handles.Pass());
      break;
    case MessageInTransit::kTypeChannel:
      OnReadMessageForChannel(message_view, platform_handles.Pass());
      break;
    default:
      HandleRemoteError(
          base::StringPrintf("Received message of invalid type %u",
                             static_cast<unsigned>(message_view.type())));
      break;
  }
}

void Channel::OnError(Error error) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  switch (error) {
    case ERROR_READ_SHUTDOWN:
      // The other side was cleanly closed, so this isn't actually an error.
      DVLOG(1) << "RawChannel read error (shutdown)";
      break;
    case ERROR_READ_BROKEN: {
      base::AutoLock locker(lock_);
      LOG_IF(ERROR, !is_shutting_down_)
          << "RawChannel read error (connection broken)";
      break;
    }
    case ERROR_READ_BAD_MESSAGE:
      // Receiving a bad message means either a bug, data corruption, or
      // malicious attack (probably due to some other bug).
      LOG(ERROR) << "RawChannel read error (received bad message)";
      break;
    case ERROR_READ_UNKNOWN:
      LOG(ERROR) << "RawChannel read error (unknown)";
      break;
    case ERROR_WRITE:
      // Write errors are slightly notable: they probably shouldn't happen under
      // normal operation (but maybe the other side crashed).
      LOG(WARNING) << "RawChannel write error";
      break;
  }
  Shutdown();
}

void Channel::OnReadMessageForDownstream(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());
  DCHECK(message_view.type() == MessageInTransit::kTypeMessagePipeEndpoint ||
         message_view.type() == MessageInTransit::kTypeMessagePipe);

  ChannelEndpointId local_id = message_view.destination_id();
  if (!local_id.is_valid()) {
    HandleRemoteError("Received message with no destination ID");
    return;
  }

  scoped_refptr<ChannelEndpoint> endpoint;
  {
    base::AutoLock locker(lock_);

    // Since we own |raw_channel_|, and this method and |Shutdown()| should only
    // be called from the creation thread, |raw_channel_| should never be null
    // here.
    DCHECK(is_running_);

    IdToEndpointMap::const_iterator it =
        local_id_to_endpoint_map_.find(local_id);
    if (it != local_id_to_endpoint_map_.end()) {
      // Ignore messages for zombie endpoints (not an error).
      if (!it->second.get()) {
        DVLOG(2) << "Ignoring downstream message for zombie endpoint (local ID "
                    "= " << local_id
                 << ", remote ID = " << message_view.source_id() << ")";
        return;
      }

      endpoint = it->second;
    }
  }
  if (!endpoint.get()) {
    HandleRemoteError(base::StringPrintf(
        "Received a message for nonexistent local destination ID %u",
        static_cast<unsigned>(local_id.value)));
    // This is strongly indicative of some problem. However, it's not a fatal
    // error, since it may indicate a buggy (or hostile) remote process. Don't
    // die even for Debug builds, since handling this properly needs to be
    // tested (TODO(vtl)).
    DLOG(ERROR) << "This should not happen under normal operation.";
    return;
  }

  if (!endpoint->OnReadMessage(message_view, platform_handles.Pass())) {
    HandleLocalError(
        base::StringPrintf("Failed to enqueue message to local ID %u",
                           static_cast<unsigned>(local_id.value)));
    return;
  }
}

void Channel::OnReadMessageForChannel(
    const MessageInTransit::View& message_view,
    embedder::ScopedPlatformHandleVectorPtr platform_handles) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());
  DCHECK_EQ(message_view.type(), MessageInTransit::kTypeChannel);

  // Currently, no channel messages take platform handles.
  if (platform_handles) {
    HandleRemoteError(
        "Received invalid channel message (has platform handles)");
    NOTREACHED();
    return;
  }

  switch (message_view.subtype()) {
    case MessageInTransit::kSubtypeChannelRunMessagePipeEndpoint:
      DVLOG(2) << "Handling channel message to run message pipe (local ID "
               << message_view.destination_id() << ", remote ID "
               << message_view.source_id() << ")";
      if (!OnRunMessagePipeEndpoint(message_view.destination_id(),
                                    message_view.source_id())) {
        HandleRemoteError(
            "Received invalid channel message to run message pipe");
      }
      break;
    case MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpoint:
      DVLOG(2) << "Handling channel message to remove message pipe (local ID "
               << message_view.destination_id() << ", remote ID "
               << message_view.source_id() << ")";
      if (!OnRemoveMessagePipeEndpoint(message_view.destination_id(),
                                       message_view.source_id())) {
        HandleRemoteError(
            "Received invalid channel message to remove message pipe");
      }
      break;
    case MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpointAck:
      DVLOG(2) << "Handling channel message to ack remove message pipe (local "
                  "ID " << message_view.destination_id() << ", remote ID "
               << message_view.source_id() << ")";
      if (!OnRemoveMessagePipeEndpointAck(message_view.destination_id())) {
        HandleRemoteError(
            "Received invalid channel message to ack remove message pipe");
      }
      break;
    default:
      HandleRemoteError("Received invalid channel message");
      NOTREACHED();
      break;
  }
}

bool Channel::OnRunMessagePipeEndpoint(ChannelEndpointId local_id,
                                       ChannelEndpointId remote_id) {
  scoped_refptr<ChannelEndpoint> endpoint;
  {
    base::AutoLock locker(lock_);

    IdToEndpointMap::iterator it = local_id_to_endpoint_map_.find(local_id);
    if (it == local_id_to_endpoint_map_.end())
      return false;

    endpoint = it->second;
  }

  RunEndpoint(endpoint, remote_id);
  return true;
}

bool Channel::OnRemoveMessagePipeEndpoint(ChannelEndpointId local_id,
                                          ChannelEndpointId remote_id) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  scoped_refptr<ChannelEndpoint> endpoint;
  {
    base::AutoLock locker(lock_);

    IdToEndpointMap::iterator it = local_id_to_endpoint_map_.find(local_id);
    if (it == local_id_to_endpoint_map_.end()) {
      DVLOG(2) << "Remove message pipe endpoint error: not found";
      return false;
    }

    if (!it->second.get()) {
      // Remove messages "crossed"; we have to wait for the ack.
      return true;
    }

    endpoint = it->second;
    local_id_to_endpoint_map_.erase(it);
    // Detach and send the remove ack message outside the lock.
  }

  endpoint->DetachFromChannel();

  if (!SendControlMessage(
          MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpointAck,
          local_id,
          remote_id)) {
    HandleLocalError(base::StringPrintf(
        "Failed to send message to remove remote message pipe endpoint ack "
        "(local ID %u, remote ID %u)",
        static_cast<unsigned>(local_id.value),
        static_cast<unsigned>(remote_id.value)));
  }

  endpoint->OnDisconnect();
  return true;
}

bool Channel::OnRemoveMessagePipeEndpointAck(ChannelEndpointId local_id) {
  DCHECK(creation_thread_checker_.CalledOnValidThread());

  base::AutoLock locker(lock_);

  IdToEndpointMap::iterator it = local_id_to_endpoint_map_.find(local_id);
  if (it == local_id_to_endpoint_map_.end()) {
    DVLOG(2) << "Remove message pipe endpoint ack error: not found";
    return false;
  }

  if (it->second.get()) {
    DVLOG(2) << "Remove message pipe endpoint ack error: wrong state";
    return false;
  }

  local_id_to_endpoint_map_.erase(it);
  return true;
}

bool Channel::SendControlMessage(MessageInTransit::Subtype subtype,
                                 ChannelEndpointId local_id,
                                 ChannelEndpointId remote_id) {
  DVLOG(2) << "Sending channel control message: subtype " << subtype
           << ", local ID " << local_id << ", remote ID " << remote_id;
  scoped_ptr<MessageInTransit> message(new MessageInTransit(
      MessageInTransit::kTypeChannel, subtype, 0, nullptr));
  message->set_source_id(local_id);
  message->set_destination_id(remote_id);
  return WriteMessage(message.Pass());
}

void Channel::HandleRemoteError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this? Probably we want to
  // terminate the connection, since it's spewing invalid stuff.
  LOG(WARNING) << error_message;
}

void Channel::HandleLocalError(const base::StringPiece& error_message) {
  // TODO(vtl): Is this how we really want to handle this?
  // Sometimes we'll want to propagate the error back to the message pipe
  // (endpoint), and notify it that the remote is (effectively) closed.
  // Sometimes we'll want to kill the channel (and notify all the endpoints that
  // their remotes are dead.
  LOG(WARNING) << error_message;
}

}  // namespace system
}  // namespace mojo
