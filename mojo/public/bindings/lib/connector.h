// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_CONNECTOR_H_
#define MOJO_PUBLIC_BINDINGS_LIB_CONNECTOR_H_

#include "mojo/public/bindings/lib/message.h"
#include "mojo/public/bindings/lib/message_queue.h"
#include "mojo/public/environment/default_async_waiter.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {
class ErrorHandler;

namespace internal {

// The Connector class is responsible for performing read/write operations on a
// MessagePipe. It writes messages it receives through the MessageReceiver
// interface that it subclasses, and it forwards messages it reads through the
// MessageReceiver interface assigned as its incoming receiver.
//
// NOTE: MessagePipe I/O is non-blocking.
//
class Connector : public MessageReceiver {
 public:
  // The Connector takes ownership of |message_pipe|.
  explicit Connector(ScopedMessagePipeHandle message_pipe,
                     MojoAsyncWaiter* waiter = GetDefaultAsyncWaiter());
  virtual ~Connector();

  // Sets the receiver to handle messages read from the message pipe.  The
  // Connector will only read messages from the pipe if an incoming receiver
  // has been set.
  void set_incoming_receiver(MessageReceiver* receiver) {
    incoming_receiver_ = receiver;
  }

  // Sets the error handler to receive notifications when an error is
  // encountered while reading from the pipe or waiting to read from the pipe.
  void set_error_handler(ErrorHandler* error_handler) {
    error_handler_ = error_handler;
  }

  // Returns true if an error was encountered while reading from the pipe or
  // waiting to read from the pipe.
  bool encountered_error() const { return error_; }

  // MessageReceiver implementation:
  virtual bool Accept(Message* message) MOJO_OVERRIDE;

 private:
  static void CallOnHandleReady(void* closure, MojoResult result);
  void OnHandleReady(MojoResult result);

  void WaitToReadMore();
  void ReadMore();

  ErrorHandler* error_handler_;
  MojoAsyncWaiter* waiter_;

  ScopedMessagePipeHandle message_pipe_;
  MessageReceiver* incoming_receiver_;

  MojoAsyncWaitID async_wait_id_;
  bool error_;
  bool drop_writes_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Connector);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_CONNECTOR_H_
