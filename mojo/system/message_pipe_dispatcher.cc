// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_pipe_dispatcher.h"

#include "base/logging.h"
#include "mojo/system/constants.h"
#include "mojo/system/memory.h"
#include "mojo/system/message_pipe.h"

namespace mojo {
namespace system {

const unsigned kInvalidPort = static_cast<unsigned>(-1);

// MessagePipeDispatcher -------------------------------------------------------

MessagePipeDispatcher::MessagePipeDispatcher()
    : port_(kInvalidPort) {
}

void MessagePipeDispatcher::Init(scoped_refptr<MessagePipe> message_pipe,
                                 unsigned port) {
  DCHECK(message_pipe.get());
  DCHECK(port == 0 || port == 1);

  message_pipe_ = message_pipe;
  port_ = port;
}

Dispatcher::Type MessagePipeDispatcher::GetType() const {
  return kTypeMessagePipe;
}

MessagePipeDispatcher::~MessagePipeDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!message_pipe_.get());
}

MessagePipe* MessagePipeDispatcher::GetMessagePipeNoLock() const {
  lock().AssertAcquired();
  return message_pipe_.get();
}

unsigned MessagePipeDispatcher::GetPortNoLock() const {
  lock().AssertAcquired();
  return port_;
}

void MessagePipeDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  message_pipe_->CancelAllWaiters(port_);
}

void MessagePipeDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  message_pipe_->Close(port_);
  message_pipe_ = NULL;
  port_ = kInvalidPort;
}

scoped_refptr<Dispatcher>
MessagePipeDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  scoped_refptr<MessagePipeDispatcher> rv = new MessagePipeDispatcher();
  rv->Init(message_pipe_, port_);
  message_pipe_ = NULL;
  port_ = kInvalidPort;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult MessagePipeDispatcher::WriteMessageImplNoLock(
    const void* bytes,
    uint32_t num_bytes,
    std::vector<DispatcherTransport>* transports,
    MojoWriteMessageFlags flags) {
  DCHECK(!transports || (transports->size() > 0 &&
                         transports->size() <= kMaxMessageNumHandles));

  lock().AssertAcquired();

  if (!VerifyUserPointer<void>(bytes, num_bytes))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_bytes > kMaxMessageNumBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return message_pipe_->WriteMessage(port_, bytes, num_bytes, transports,
                                     flags);
}

MojoResult MessagePipeDispatcher::ReadMessageImplNoLock(
    void* bytes,
    uint32_t* num_bytes,
    std::vector<scoped_refptr<Dispatcher> >* dispatchers,
    uint32_t* num_dispatchers,
    MojoReadMessageFlags flags) {
  lock().AssertAcquired();

  if (num_bytes) {
    if (!VerifyUserPointer<uint32_t>(num_bytes, 1))
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (!VerifyUserPointer<void>(bytes, *num_bytes))
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  return message_pipe_->ReadMessage(port_, bytes, num_bytes, dispatchers,
                                    num_dispatchers, flags);
}

MojoResult MessagePipeDispatcher::AddWaiterImplNoLock(Waiter* waiter,
                                                      MojoWaitFlags flags,
                                                      MojoResult wake_result) {
  lock().AssertAcquired();
  return message_pipe_->AddWaiter(port_, waiter, flags, wake_result);
}

void MessagePipeDispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock().AssertAcquired();
  message_pipe_->RemoveWaiter(port_, waiter);
}

// MessagePipeDispatcherTransport ----------------------------------------------

MessagePipeDispatcherTransport::MessagePipeDispatcherTransport(
    DispatcherTransport transport) : DispatcherTransport(transport) {
  DCHECK_EQ(message_pipe_dispatcher()->GetType(), Dispatcher::kTypeMessagePipe);
}

}  // namespace system
}  // namespace mojo
