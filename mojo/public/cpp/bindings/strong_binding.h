// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_

#include "mojo/public/c/environment/async_waiter.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/lib/filter_chain.h"
#include "mojo/public/cpp/bindings/lib/message_header_validator.h"
#include "mojo/public/cpp/bindings/lib/router.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {

// This connects an interface implementation strongly to a pipe. When a
// connection error is detected the implementation is deleted. Deleting the
// connector also closes the pipe.
//
// Example of an implementation that is always bound strongly to a pipe
//
//   class StronglyBound : public Foo {
//    public:
//     explicit StronglyBound(ScopedMessagePipeHandle handle)
//         : binding_(this, handle.Pass()) {}
//
//     // Foo implementation here
//
//    private:
//     StrongBinding<Foo> binding_;
//   };
//
template <typename Interface>
class StrongBinding : public ErrorHandler {
 public:
  explicit StrongBinding(Interface* impl) : binding_(impl) {
    binding_.set_error_handler(this);
  }

  StrongBinding(
      Interface* impl,
      ScopedMessagePipeHandle handle,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter())
      : StrongBinding(impl) {
    binding_.Bind(handle.Pass(), waiter);
  }

  StrongBinding(
      Interface* impl,
      InterfaceRequest<Interface> request,
      const MojoAsyncWaiter* waiter = Environment::GetDefaultAsyncWaiter())
      : StrongBinding(impl) {
    binding_.Bind(request.Pass(), waiter);
  }

  ~StrongBinding() override {}

  bool WaitForIncomingMethodCall() {
    return binding_.WaitForIncomingMethodCall();
  }

  void set_error_handler(ErrorHandler* error_handler) {
    error_handler_ = error_handler;
  }

  typename Interface::Client* client() { return binding_.client(); }
  // Exposed for testing, should not generally be used.
  internal::Router* internal_router() { return binding_.internal_router(); }

  // ErrorHandler implementation
  void OnConnectionError() override {
    if (error_handler_)
      error_handler_->OnConnectionError();
    delete binding_.impl();
  }

 private:
  ErrorHandler* error_handler_ = nullptr;
  Binding<Interface> binding_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRONG_BINDING_H_
