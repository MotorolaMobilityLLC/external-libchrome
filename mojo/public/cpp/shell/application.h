// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SHELL_APPLICATION_H_
#define MOJO_PUBLIC_SHELL_APPLICATION_H_

#include <vector>

#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/cpp/shell/service.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"

namespace mojo {

class Application : public internal::ServiceFactoryBase::Owner {
 public:
  explicit Application(ScopedShellHandle shell_handle);
  explicit Application(MojoHandle shell_handle);
  virtual ~Application();

  // internal::ServiceFactoryBase::Owner methods.
  // Takes ownership of |service_factory|.
  virtual void AddServiceFactory(internal::ServiceFactoryBase* service_factory)
      MOJO_OVERRIDE;
  virtual void RemoveServiceFactory(
    internal::ServiceFactoryBase* service_factory) MOJO_OVERRIDE;

 protected:
  // ShellClient methods.
  virtual void AcceptConnection(const mojo::String& url,
                                ScopedMessagePipeHandle client_handle)
      MOJO_OVERRIDE;

 private:
  typedef std::vector<internal::ServiceFactoryBase*> ServiceFactoryList;
  ServiceFactoryList service_factories_;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_SHELL_APPLICATION_H_
