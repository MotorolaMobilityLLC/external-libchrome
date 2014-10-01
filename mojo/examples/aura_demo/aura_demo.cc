// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/aura/context_factory_mojo.h"
#include "mojo/aura/screen_mojo.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/aura/window_tree_host_mojo_delegate.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_client_factory.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/codec/png_codec.h"

namespace examples {

// Trivial WindowDelegate implementation that draws a colored background.
class DemoWindowDelegate : public aura::WindowDelegate {
 public:
  explicit DemoWindowDelegate(SkColor color) : color_(color) {}

  // Overridden from WindowDelegate:
  virtual gfx::Size GetMinimumSize() const override {
    return gfx::Size();
  }

  virtual gfx::Size GetMaximumSize() const override {
    return gfx::Size();
  }

  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) override {}
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::kNullCursor;
  }
  virtual int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCAPTION;
  }
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  virtual bool CanFocus() override { return true; }
  virtual void OnCaptureLost() override {}
  virtual void OnPaint(gfx::Canvas* canvas) override {
    canvas->DrawColor(color_, SkXfermode::kSrc_Mode);
  }
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) override {}
  virtual void OnWindowDestroying(aura::Window* window) override {}
  virtual void OnWindowDestroyed(aura::Window* window) override {}
  virtual void OnWindowTargetVisibilityChanged(bool visible) override {}
  virtual bool HasHitTestMask() const override { return false; }
  virtual void GetHitTestMask(gfx::Path* mask) const override {}

 private:
  const SkColor color_;

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
                                         const gfx::Rect& bounds) override {
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

class AuraDemo : public mojo::ApplicationDelegate,
                 public mojo::WindowTreeHostMojoDelegate,
                 public mojo::ViewManagerDelegate {
 public:
  AuraDemo() : window1_(NULL), window2_(NULL), window21_(NULL) {}
  virtual ~AuraDemo() {}

 private:
  // Overridden from ViewManagerDelegate:
  virtual void OnEmbed(
      mojo::ViewManager* view_manager,
      mojo::View* root,
      mojo::ServiceProviderImpl* exported_services,
      scoped_ptr<mojo::ServiceProvider> imported_services) override {
    // TODO(beng): this function could be called multiple times!
    root_ = root;

    window_tree_host_.reset(new mojo::WindowTreeHostMojo(root, this));
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
  virtual void OnViewManagerDisconnected(
      mojo::ViewManager* view_manager) override {
    base::MessageLoop::current()->Quit();
  }

  // WindowTreeHostMojoDelegate:
  virtual void CompositorContentsChanged(const SkBitmap& bitmap) override {
    root_->SetContents(bitmap);
  }

  virtual void Initialize(mojo::ApplicationImpl* app) override {
    view_manager_client_factory_.reset(
        new mojo::ViewManagerClientFactory(app->shell(), this));
    aura::Env::CreateInstance(true);
    context_factory_.reset(new mojo::ContextFactoryMojo);
    aura::Env::GetInstance()->set_context_factory(context_factory_.get());
    screen_.reset(mojo::ScreenMojo::Create());
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  }

  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(view_manager_client_factory_.get());
    return true;
  }

  scoped_ptr<DemoWindowTreeClient> window_tree_client_;

  scoped_ptr<ui::ContextFactory> context_factory_;

  scoped_ptr<mojo::ScreenMojo> screen_;

  scoped_ptr<DemoWindowDelegate> delegate1_;
  scoped_ptr<DemoWindowDelegate> delegate2_;
  scoped_ptr<DemoWindowDelegate> delegate21_;

  aura::Window* window1_;
  aura::Window* window2_;
  aura::Window* window21_;

  mojo::View* root_;

  scoped_ptr<mojo::ViewManagerClientFactory> view_manager_client_factory_;

  scoped_ptr<aura::WindowTreeHost> window_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(AuraDemo);
};

}  // namespace examples

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new examples::AuraDemo);
  return runner.Run(shell_handle);
}
