// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_APPLICATION_MANAGER_SHELL_IMPL_H_
#define SHELL_APPLICATION_MANAGER_SHELL_IMPL_H_

#include "base/callback.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/error_handler.h"
#include "mojo/public/interfaces/application/application.mojom.h"
#include "mojo/public/interfaces/application/shell.mojom.h"
#include "mojo/shell/application_manager/identity.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ApplicationManager;

class ShellImpl : public Shell, public ErrorHandler {
 public:
  ShellImpl(ApplicationPtr application,
            ApplicationManager* manager,
            const Identity& resolved_identity,
            const base::Closure& on_application_end);

  ~ShellImpl() override;

  void InitializeApplication(Array<String> args);

  void ConnectToClient(const GURL& requested_url,
                       const GURL& requestor_url,
                       InterfaceRequest<ServiceProvider> services,
                       ServiceProviderPtr exposed_services);

  Application* application() { return application_.get(); }
  const Identity& identity() const { return identity_; }
  base::Closure on_application_end() const { return on_application_end_; }

 private:
  // Shell implementation:
  void ConnectToApplication(const String& app_url,
                            InterfaceRequest<ServiceProvider> services,
                            ServiceProviderPtr exposed_services) override;

  // ErrorHandler implementation:
  void OnConnectionError() override;

  ApplicationManager* const manager_;
  const Identity identity_;
  base::Closure on_application_end_;
  ApplicationPtr application_;
  Binding<Shell> binding_;

  DISALLOW_COPY_AND_ASSIGN(ShellImpl);
};

}  // namespace shell
}  // namespace mojo

#endif  // SHELL_APPLICATION_MANAGER_SHELL_IMPL_H_
