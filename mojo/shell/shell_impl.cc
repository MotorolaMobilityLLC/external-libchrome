// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell_impl.h"

#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/common/common_type_converters.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/shell/application_manager.h"

namespace mojo {
namespace shell {

ShellImpl::ShellImpl(ApplicationPtr application,
                     ApplicationManager* manager,
                     const Identity& identity,
                     const base::Closure& on_application_end)
    : manager_(manager),
      identity_(identity),
      on_application_end_(on_application_end),
      application_(application.Pass()),
      binding_(this) {
  binding_.set_error_handler(this);
}

ShellImpl::~ShellImpl() {
}

void ShellImpl::InitializeApplication() {
  ShellPtr shell;
  binding_.Bind(GetProxy(&shell));
  application_->Initialize(shell.Pass(), identity_.url.spec());
}

void ShellImpl::ConnectToClient(const GURL& requested_url,
                                const GURL& requestor_url,
                                InterfaceRequest<ServiceProvider> services,
                                ServiceProviderPtr exposed_services) {
  application_->AcceptConnection(String::From(requestor_url), services.Pass(),
                                 exposed_services.Pass(), requested_url.spec());
}

// Shell implementation:
void ShellImpl::ConnectToApplication(mojo::URLRequestPtr app_request,
                                     InterfaceRequest<ServiceProvider> services,
                                     ServiceProviderPtr exposed_services) {
  GURL app_gurl(app_request->url.To<std::string>());
  if (!app_gurl.is_valid()) {
    LOG(ERROR) << "Error: invalid URL: " << app_request;
    return;
  }
  manager_->ConnectToApplication(app_request.Pass(), identity_.url,
                                 services.Pass(), exposed_services.Pass(),
                                 base::Closure());
}

void ShellImpl::OnConnectionError() {
  manager_->OnShellImplError(this);
}

}  // namespace shell
}  // namespace mojo
