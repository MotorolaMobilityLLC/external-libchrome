// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/local_message_pipe_endpoint.h"

#include <string.h>

#include "base/logging.h"
#include "mojo/system/dispatcher.h"
#include "mojo/system/message_in_transit.h"

namespace mojo {
namespace system {

LocalMessagePipeEndpoint::LocalMessagePipeEndpoint()
    : is_open_(true),
      is_peer_open_(true) {
}

LocalMessagePipeEndpoint::~LocalMessagePipeEndpoint() {
  DCHECK(!is_open_);
  DCHECK(message_queue_.IsEmpty());  // Should be implied by not being open.
}

MessagePipeEndpoint::Type LocalMessagePipeEndpoint::GetType() const {
  return kTypeLocal;
}

bool LocalMessagePipeEndpoint::OnPeerClose() {
  DCHECK(is_open_);
  DCHECK(is_peer_open_);

  WaitFlagsState old_state = GetWaitFlagsState();
  is_peer_open_ = false;
  WaitFlagsState new_state = GetWaitFlagsState();

  if (!new_state.equals(old_state))
    waiter_list_.AwakeWaitersForStateChange(new_state);

  return true;
}

void LocalMessagePipeEndpoint::EnqueueMessage(
    scoped_ptr<MessageInTransit> message) {
  DCHECK(is_open_);
  DCHECK(is_peer_open_);

  bool was_empty = message_queue_.IsEmpty();
  message_queue_.AddMessage(message.Pass());
  if (was_empty)
    waiter_list_.AwakeWaitersForStateChange(GetWaitFlagsState());
}

void LocalMessagePipeEndpoint::Close() {
  DCHECK(is_open_);
  is_open_ = false;
  message_queue_.Clear();
}

void LocalMessagePipeEndpoint::CancelAllWaiters() {
  DCHECK(is_open_);
  waiter_list_.CancelAllWaiters();
}

MojoResult LocalMessagePipeEndpoint::ReadMessage(void* bytes,
                                                 uint32_t* num_bytes,
                                                 DispatcherVector* dispatchers,
                                                 uint32_t* num_dispatchers,
                                                 MojoReadMessageFlags flags) {
  DCHECK(is_open_);
  DCHECK(!dispatchers || dispatchers->empty());

  const uint32_t max_bytes = num_bytes ? *num_bytes : 0;
  const uint32_t max_num_dispatchers = num_dispatchers ? *num_dispatchers : 0;

  if (message_queue_.IsEmpty()) {
    return is_peer_open_ ? MOJO_RESULT_SHOULD_WAIT :
                           MOJO_RESULT_FAILED_PRECONDITION;
  }

  // TODO(vtl): If |flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD|, we could pop
  // and release the lock immediately.
  bool enough_space = true;
  MessageInTransit* message = message_queue_.PeekMessage();
  if (num_bytes)
    *num_bytes = message->num_bytes();
  if (message->num_bytes() <= max_bytes)
    memcpy(bytes, message->bytes(), message->num_bytes());
  else
    enough_space = false;

  if (DispatcherVector* queued_dispatchers = message->dispatchers()) {
    if (num_dispatchers)
      *num_dispatchers = static_cast<uint32_t>(queued_dispatchers->size());
    if (enough_space) {
      if (queued_dispatchers->empty()) {
        // Nothing to do.
      } else if (queued_dispatchers->size() <= max_num_dispatchers) {
        DCHECK(dispatchers);
        dispatchers->swap(*queued_dispatchers);
      } else {
        enough_space = false;
      }
    }
  } else {
    if (num_dispatchers)
      *num_dispatchers = 0;
  }

  message = NULL;

  if (enough_space || (flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD)) {
    message_queue_.DiscardMessage();

    // Now it's empty, thus no longer readable.
    if (message_queue_.IsEmpty()) {
      // It's currently not possible to wait for non-readability, but we should
      // do the state change anyway.
      waiter_list_.AwakeWaitersForStateChange(GetWaitFlagsState());
    }
  }

  if (!enough_space)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return MOJO_RESULT_OK;
}

MojoResult LocalMessagePipeEndpoint::AddWaiter(Waiter* waiter,
                                               MojoWaitFlags flags,
                                               MojoResult wake_result) {
  DCHECK(is_open_);

  WaitFlagsState state = GetWaitFlagsState();
  if (state.satisfies(flags))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!state.can_satisfy(flags))
    return MOJO_RESULT_FAILED_PRECONDITION;

  waiter_list_.AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void LocalMessagePipeEndpoint::RemoveWaiter(Waiter* waiter) {
  DCHECK(is_open_);
  waiter_list_.RemoveWaiter(waiter);
}

WaitFlagsState LocalMessagePipeEndpoint::GetWaitFlagsState() {
  WaitFlagsState rv;
  if (!message_queue_.IsEmpty()) {
    rv.satisfied_flags |= MOJO_WAIT_FLAG_READABLE;
    rv.satisfiable_flags |= MOJO_WAIT_FLAG_READABLE;
  }
  if (is_peer_open_) {
    rv.satisfied_flags |= MOJO_WAIT_FLAG_WRITABLE;
    rv.satisfiable_flags |= MOJO_WAIT_FLAG_READABLE | MOJO_WAIT_FLAG_WRITABLE;
  }
  return rv;
}

}  // namespace system
}  // namespace mojo
