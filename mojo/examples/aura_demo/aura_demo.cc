// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/aura/screen_mojo.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/native_viewport/native_viewport.mojom.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"

#if defined(WIN32)
#if !defined(CDECL)
#define CDECL __cdecl
#endif
#define AURA_DEMO_EXPORT __declspec(dllexport)
#else
#define CDECL
#define AURA_DEMO_EXPORT __attribute__((visibility("default")))
#endif

namespace mojo {
namespace examples {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  // Overridden from WindowDelegate:
  virtual gfx::Size GetMinimumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual gfx::Size GetMaximumSize() const OVERRIDE {
    return gfx::Size();
  }

  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE {}
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE {
    return HTCAPTION;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE {
    return true;
  }
  virtual bool CanFocus() OVERRIDE { return true; }
  virtual void OnCaptureLost() OVERRIDE {}
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {}
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {}
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE {}
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE {}
  virtual bool HasHitTestMask() const OVERRIDE { return false; }
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE {}

 private:
  SkColor color_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowDelegate);
};

class DemoWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit DemoWindowTreeClient(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  virtual ~DemoWindowTreeClient() {
    aura::client::SetWindowTreeClient(window_, NULL);
  }

  // Overridden from aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    if (!capture_client_) {
      capture_client_.reset(
          new aura::client::DefaultCaptureClient(window_->GetRootWindow()));
    }
    return window_;
  }

 private:
  aura::Window* window_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(DemoWindowTreeClient);
};

class AuraDemo : public Application {
 public:
  explicit AuraDemo(MojoHandle service_provider_handle)
      : Application(service_provider_handle) {
    screen_.reset(ScreenMojo::Create());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());

    NativeViewportPtr native_viewport;
    ConnectTo("mojo:mojo_native_viewport_service", &native_viewport);

    window_tree_host_.reset(new WindowTreeHostMojo(
        native_viewport.Pass(),
        gfx::Rect(800, 600),
        base::Bind(&AuraDemo::HostContextCreated, base::Unretained(this))));
  }

 private:
  void HostContextCreated() {
    window_tree_host_->InitHost();

    window_tree_client_.reset(
        new DemoWindowTreeClient(window_tree_host_->window()));

    delegate1_.reset(new DemoWindowDelegate(SK_ColorBLUE));
    window1_ = new aura::Window(delegate1_.get());
    window1_->Init(aura::WINDOW_LAYER_TEXTURED);
    window1_->SetBounds(gfx::Rect(100, 100, 400, 400));
    window1_->Show();
    window_tree_host_->window()->AddChild(window1_);

    delegate2_.reset(new DemoWindowDelegate(SK_ColorRED));
    window2_ = new aura::Window(delegate2_.get());
    window2_->Init(aura::WINDOW_LAYER_TEXTURED);
    window2_->SetBounds(gfx::Rect(200, 200, 350, 350));
    window2_->Show();
    window_tree_host_->window()->AddChild(window2_);

    delegate21_.reset(new DemoWindowDelegate(SK_ColorGREEN));
    window21_ = new aura::Window(delegate21_.get());
    window21_->Init(aura::WINDOW_LAYER_TEXTURED);
    window21_->SetBounds(gfx::Rect(10, 10, 50, 50));
    window21_->Show();
    window2_->AddChild(window21_);

    window_tree_host_->Show();
  }

  scoped_ptr<ScreenMojo> screen_;

  scoped_ptr<DemoWindowTreeClient> window_tree_client_;

  scoped_ptr<DemoWindowDelegate> delegate1_;
  scoped_ptr<DemoWindowDelegate> delegate2_;
  scoped_ptr<DemoWindowDelegate> delegate21_;

  aura::Window* window1_;
  aura::Window* window2_;
  aura::Window* window21_;

  scoped_ptr<aura::WindowTreeHost> window_tree_host_;
};

}  // namespace examples
}  // namespace mojo

extern "C" AURA_DEMO_EXPORT MojoResult CDECL MojoMain(
    MojoHandle service_provider_handle) {
  base::CommandLine::Init(0, NULL);
  base::AtExitManager at_exit;
  base::MessageLoop loop;
  mojo::GLES2Initializer gles2;

  // TODO(beng): This crashes in a DCHECK on X11 because this thread's
  //             MessageLoop is not of TYPE_UI. I think we need a way to build
  //             Aura that doesn't define platform-specific stuff.
  aura::Env::CreateInstance(true);
  mojo::examples::AuraDemo app(service_provider_handle);
  loop.Run();

  return MOJO_RESULT_OK;
}
