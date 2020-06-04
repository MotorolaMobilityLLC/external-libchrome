// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_SHELL_H_
#define MOJO_SHELL_PUBLIC_CPP_SHELL_H_

#include "mojo/shell/public/cpp/app_lifetime_helper.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

namespace mojo {

shell::mojom::CapabilityFilterPtr CreatePermissiveCapabilityFilter();

using ShellClientRequest = InterfaceRequest<shell::mojom::ShellClient>;

class Shell {
 public:
  class ConnectParams {
   public:
    explicit ConnectParams(const std::string& url);
    explicit ConnectParams(URLRequestPtr request);
    ~ConnectParams();

    URLRequestPtr TakeRequest() { return std::move(request_); }
    shell::mojom::CapabilityFilterPtr TakeFilter() {
      return std::move(filter_);
    }
    void set_filter(shell::mojom::CapabilityFilterPtr filter) {
      filter_ = std::move(filter);
    }

   private:
    URLRequestPtr request_;
    shell::mojom::CapabilityFilterPtr filter_;

    DISALLOW_COPY_AND_ASSIGN(ConnectParams);
  };

  // Requests a new connection to an application. Returns a pointer to the
  // connection if the connection is permitted by this application's delegate,
  // or nullptr otherwise. Caller takes ownership.
  virtual scoped_ptr<Connection> Connect(const std::string& url) = 0;
  virtual scoped_ptr<Connection> Connect(ConnectParams* params) = 0;

  // Connect to application identified by |request->url| and connect to the
  // service implementation of the interface identified by |Interface|.
  template <typename Interface>
  void ConnectToService(ConnectParams* params, InterfacePtr<Interface>* ptr) {
    scoped_ptr<Connection> connection = Connect(params);
    if (!connection.get())
      return;
    connection->ConnectToService(ptr);
  }
  template <typename Interface>
  void ConnectToService(const std::string& url, InterfacePtr<Interface>* ptr) {
    ConnectParams params(url);
    params.set_filter(CreatePermissiveCapabilityFilter());
    return ConnectToService(&params, ptr);
  }

  // Initiate shutdown of this application. This may involve a round trip to the
  // Shell to ensure there are no inbound service requests.
  virtual void Quit() = 0;

  // Create an object that can be used to refcount the lifetime of the
  // application. The returned object may be cloned, and when the refcount falls
  // to zero Quit() is called.
  virtual scoped_ptr<AppRefCount> CreateAppRefCount() = 0;
};

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_H_
