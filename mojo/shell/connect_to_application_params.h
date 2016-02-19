// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_CONNECT_TO_APPLICATION_PARAMS_H_
#define MOJO_SHELL_CONNECT_TO_APPLICATION_PARAMS_H_

#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/public/interfaces/interface_provider.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationInstance;

// This class represents a request for the application manager to connect to an
// application.
class ConnectToApplicationParams {
 public:
  ConnectToApplicationParams();
  ~ConnectToApplicationParams();

  // Sets |source_|. If |source| is null, |source_| is reset.
  void SetSource(ApplicationInstance* source);

  // The following methods set both |target_| and |target_url_request_|.
  void SetTargetURL(const GURL& target_url);

  void set_source(const Identity& source) { source_ = source;  }
  const Identity& source() const { return source_; }
  void set_target(const Identity& target) { target_ = target; }
  const Identity& target() const { return target_; }

  void set_remote_interfaces(shell::mojom::InterfaceProviderRequest value) {
    remote_interfaces_ = std::move(value);
  }
  shell::mojom::InterfaceProviderRequest TakeRemoteInterfaces() {
    return std::move(remote_interfaces_);
  }

  void set_local_interfaces(shell::mojom::InterfaceProviderPtr value) {
    local_interfaces_ = std::move(value);
  }
  shell::mojom::InterfaceProviderPtr TakeLocalInterfaces() {
    return std::move(local_interfaces_);
  }

  void set_on_application_end(const base::Closure& value) {
    on_application_end_ = value;
  }
  const base::Closure& on_application_end() const {
    return on_application_end_;
  }

  void set_connect_callback(
      const shell::mojom::Shell::ConnectToApplicationCallback& value) {
    connect_callback_ = value;
  }
  const shell::mojom::Shell::ConnectToApplicationCallback&
      connect_callback() const {
    return connect_callback_;
  }

 private:
  // It may be null (i.e., is_null() returns true) which indicates that there is
  // no source (e.g., for the first application or in tests).
  Identity source_;
  // The identity of the application being connected to.
  Identity target_;

  shell::mojom::InterfaceProviderRequest remote_interfaces_;
  shell::mojom::InterfaceProviderPtr local_interfaces_;
  base::Closure on_application_end_;
  shell::mojom::Shell::ConnectToApplicationCallback connect_callback_;

  DISALLOW_COPY_AND_ASSIGN(ConnectToApplicationParams);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_CONNECT_TO_APPLICATION_PARAMS_H_
