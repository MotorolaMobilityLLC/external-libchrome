// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/local_message_pipe_endpoint.h"

#include <string.h>

#include "base/logging.h"
#include "base/stl_util.h"
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
  DCHECK(message_queue_.empty());  // Should be implied by not being open.
}

void LocalMessagePipeEndpoint::Close() {
  DCHECK(is_open_);
  is_open_ = false;

  STLDeleteElements(&message_queue_);
}

void LocalMessagePipeEndpoint::OnPeerClose() {
  DCHECK(is_open_);
  DCHECK(is_peer_open_);

  MojoWaitFlags old_satisfied_flags = SatisfiedFlags();
  MojoWaitFlags old_satisfiable_flags = SatisfiableFlags();
  is_peer_open_ = false;
  MojoWaitFlags new_satisfied_flags = SatisfiedFlags();
  MojoWaitFlags new_satisfiable_flags = SatisfiableFlags();

  if (new_satisfied_flags != old_satisfied_flags ||
      new_satisfiable_flags != old_satisfiable_flags) {
    waiter_list_.AwakeWaitersForStateChange(new_satisfied_flags,
                                            new_satisfiable_flags);
  }
}

MojoResult LocalMessagePipeEndpoint::EnqueueMessage(
    scoped_ptr<MessageInTransit> message,
    std::vector<DispatcherTransport>* transports) {
  DCHECK(is_open_);
  DCHECK(is_peer_open_);
  DCHECK(!transports || !transports->empty());

  // "Move" the dispatchers
  if (transports) {
    scoped_ptr<std::vector<scoped_refptr<Dispatcher> > >
        dispatchers(new std::vector<scoped_refptr<Dispatcher> >());
    dispatchers->reserve(transports->size());
    for (size_t i = 0; i < transports->size(); i++) {
      if ((*transports)[i].is_valid()) {
        dispatchers->push_back(
            (*transports)[i].CreateEquivalentDispatcherAndClose());
      } else {
        LOG(WARNING) << "Enqueueing null dispatcher";
        dispatchers->push_back(scoped_refptr<Dispatcher>());
      }
    }
    message->SetDispatchers(dispatchers.Pass());
  }

  bool was_empty = message_queue_.empty();
  message_queue_.push_back(message.release());
  if (was_empty) {
    waiter_list_.AwakeWaitersForStateChange(SatisfiedFlags(),
                                            SatisfiableFlags());
  }

  return MOJO_RESULT_OK;
}

void LocalMessagePipeEndpoint::CancelAllWaiters() {
  DCHECK(is_open_);
  waiter_list_.CancelAllWaiters();
}

MojoResult LocalMessagePipeEndpoint::ReadMessage(
    void* bytes, uint32_t* num_bytes,
    std::vector<scoped_refptr<Dispatcher> >* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  DCHECK(is_open_);
  DCHECK(!dispatchers || dispatchers->empty());

  const uint32_t max_bytes = num_bytes ? *num_bytes : 0;
  const uint32_t max_num_dispatchers = num_dispatchers ? *num_dispatchers : 0;

  if (message_queue_.empty()) {
    return is_peer_open_ ? MOJO_RESULT_SHOULD_WAIT :
                           MOJO_RESULT_FAILED_PRECONDITION;
  }

  // TODO(vtl): If |flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD|, we could pop
  // and release the lock immediately.
  bool enough_space = true;
  MessageInTransit* message = message_queue_.front();
  if (num_bytes)
    *num_bytes = message->num_bytes();
  if (message->num_bytes() <= max_bytes)
    memcpy(bytes, message->bytes(), message->num_bytes());
  else
    enough_space = false;

  if (std::vector<scoped_refptr<Dispatcher> >* queued_dispatchers =
          message->dispatchers()) {
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

  if (enough_space || (flags & MOJO_READ_MESSAGE_FLAG_MAY_DISCARD)) {
    delete message;
    message_queue_.pop_front();

    // Now it's empty, thus no longer readable.
    if (message_queue_.empty()) {
      // It's currently not possible to wait for non-readability, but we should
      // do the state change anyway.
      waiter_list_.AwakeWaitersForStateChange(SatisfiedFlags(),
                                              SatisfiableFlags());
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

  if ((flags & SatisfiedFlags()))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!(flags & SatisfiableFlags()))
    return MOJO_RESULT_FAILED_PRECONDITION;

  waiter_list_.AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void LocalMessagePipeEndpoint::RemoveWaiter(Waiter* waiter) {
  DCHECK(is_open_);
  waiter_list_.RemoveWaiter(waiter);
}

MojoWaitFlags LocalMessagePipeEndpoint::SatisfiedFlags() {
  MojoWaitFlags satisfied_flags = 0;
  if (!message_queue_.empty())
    satisfied_flags |= MOJO_WAIT_FLAG_READABLE;
  if (is_peer_open_)
    satisfied_flags |= MOJO_WAIT_FLAG_WRITABLE;
  return satisfied_flags;
}

MojoWaitFlags LocalMessagePipeEndpoint::SatisfiableFlags() {
  MojoWaitFlags satisfiable_flags = 0;
  if (!message_queue_.empty() || is_peer_open_)
    satisfiable_flags |= MOJO_WAIT_FLAG_READABLE;
  if (is_peer_open_)
    satisfiable_flags |= MOJO_WAIT_FLAG_WRITABLE;
  return satisfiable_flags;
}

}  // namespace system
}  // namespace mojo
