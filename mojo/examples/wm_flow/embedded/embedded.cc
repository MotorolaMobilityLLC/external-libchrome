// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/examples/bitmap_uploader/bitmap_uploader.h"
#include "mojo/examples/wm_flow/app/embedder.mojom.h"
#include "mojo/examples/wm_flow/embedded/embeddee.mojom.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/connect.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"
#include "mojo/public/cpp/application/service_provider_impl.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"

namespace examples {

namespace {

class EmbeddeeImpl : public mojo::InterfaceImpl<Embeddee> {
 public:
  EmbeddeeImpl() {}
  virtual ~EmbeddeeImpl() {}

 private:
  // Overridden from Embeddee:
  virtual void HelloBack(const mojo::Callback<void()>& callback) override {
    callback.Run();
  }

  DISALLOW_COPY_AND_ASSIGN(EmbeddeeImpl);
};

}  // namespace

class WMFlowEmbedded : public mojo::ApplicationDelegate,
                       public mojo::ViewManagerDelegate {
 public:
  WMFlowEmbedded() : shell_(nullptr) {}
  virtual ~WMFlowEmbedded() {}

 private:
  // Overridden from Application:
  virtual void Initialize(mojo::ApplicationImpl* app) override {
    shell_ = app->shell();
    view_manager_client_factory_.reset(
        new mojo::ViewManagerClientFactory(app->shell(), this));
  }
  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  // Overridden from mojo::ViewManagerDelegate:
  virtual void OnEmbed(
      mojo::ViewManager* view_manager,
      mojo::View* root,
      mojo::ServiceProviderImpl* exported_services,
      scoped_ptr<mojo::ServiceProvider> imported_services) override {
    bitmap_uploader_.reset(new mojo::BitmapUploader(root));
    bitmap_uploader_->Init(shell_);
    bitmap_uploader_->SetColor(SK_ColorMAGENTA);

    exported_services->AddService(&embeddee_factory_);
    mojo::ConnectToService(imported_services.get(), &embedder_);
    embedder_->HelloWorld(base::Bind(&WMFlowEmbedded::HelloWorldAck,
                                     base::Unretained(this)));
  }
  virtual void OnViewManagerDisconnected(
      mojo::ViewManager* view_manager) override {}

  void HelloWorldAck() {
    printf("HelloWorld() ack'ed\n");
  }

  mojo::Shell* shell_;
  scoped_ptr<mojo::ViewManagerClientFactory> view_manager_client_factory_;
  EmbedderPtr embedder_;
  mojo::InterfaceFactoryImpl<EmbeddeeImpl> embeddee_factory_;
  scoped_ptr<mojo::BitmapUploader> bitmap_uploader_;

  DISALLOW_COPY_AND_ASSIGN(WMFlowEmbedded);
};

}  // namespace examples

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new examples::WMFlowEmbedded);
  return runner.Run(shell_handle);
}

