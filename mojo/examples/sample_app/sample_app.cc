// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "mojo/examples/sample_app/gles2_client_impl.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"

namespace mojo {
namespace examples {

class SampleApp : public Application, public NativeViewportClient {
 public:
  SampleApp() {}

  virtual ~SampleApp() {
    // TODO(darin): Fix shutdown so we don't need to leak this.
    MOJO_ALLOW_UNUSED GLES2ClientImpl* leaked = gles2_client_.release();
  }

  virtual void Initialize() MOJO_OVERRIDE {
    ConnectTo("mojo:mojo_native_viewport_service", &viewport_);
    viewport_.set_client(this);

    AllocationScope scope;

    Rect::Builder rect;
    Point::Builder point;
    point.set_x(10);
    point.set_y(10);
    rect.set_position(point.Finish());
    Size::Builder size;
    size.set_width(800);
    size.set_height(600);
    rect.set_size(size.Finish());
    viewport_->Create(rect.Finish());
    viewport_->Show();

    MessagePipe gles2_pipe;
    viewport_->CreateGLES2Context(gles2_pipe.handle0.Pass());
    gles2_client_.reset(new GLES2ClientImpl(gles2_pipe.handle1.Pass()));
  }

  virtual void OnCreated() MOJO_OVERRIDE {
  }

  virtual void OnDestroyed() MOJO_OVERRIDE {
    RunLoop::current()->Quit();
  }

  virtual void OnBoundsChanged(const Rect& bounds) MOJO_OVERRIDE {
    gles2_client_->SetSize(bounds.size());
  }

  virtual void OnEvent(const Event& event,
                       const Callback<void()>& callback) MOJO_OVERRIDE {
    if (!event.location().is_null())
      gles2_client_->HandleInputEvent(event);
    callback.Run();
  }

 private:
  mojo::GLES2Initializer gles2;
  scoped_ptr<GLES2ClientImpl> gles2_client_;
  NativeViewportPtr viewport_;

  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::SampleApp();
}

}  // namespace mojo
