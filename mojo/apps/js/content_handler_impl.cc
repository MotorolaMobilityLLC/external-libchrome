// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/apps/js/content_handler_impl.h"

#include "mojo/apps/js/application_delegate_impl.h"
#include "mojo/apps/js/js_app.h"
#include "mojo/common/data_pipe_utils.h"

namespace mojo {
namespace apps {

class ContentHandlerJSApp : public JSApp {
 public:
  ContentHandlerJSApp(ApplicationDelegateImpl* app_delegate_impl,
                      URLResponsePtr content)
      : JSApp(app_delegate_impl),
        content_(content.Pass()) {
  }

  virtual bool Load(std::string* source, std::string* file_name) override {
    *file_name = content_->url;
    if (content_.is_null())
      return false;
    return common::BlockingCopyToString(content_->body.Pass(), source);
  }

 private:
  URLResponsePtr content_;
};


ContentHandlerImpl::ContentHandlerImpl(ApplicationDelegateImpl* app)
    : app_delegate_impl_(app) {
}

ContentHandlerImpl::~ContentHandlerImpl() {
}

void ContentHandlerImpl::OnConnect(
    const mojo::String& requestor_url,
    URLResponsePtr content,
    InterfaceRequest<ServiceProvider> service_provider) {
  scoped_ptr<JSApp> js_app(
      new ContentHandlerJSApp(app_delegate_impl_, content.Pass()));
  app_delegate_impl_->StartJSApp(js_app.Pass());
}

}  // namespace apps
}  // namespace mojo
