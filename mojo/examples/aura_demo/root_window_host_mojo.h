// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_AURA_DEMO_ROOT_WINDOW_HOST_MOJO_H_
#define MOJO_EXAMPLES_AURA_DEMO_ROOT_WINDOW_HOST_MOJO_H_

#include "base/bind.h"
#include "mojo/public/bindings/lib/remote_ptr.h"
#include "mojom/native_viewport.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/rect.h"

namespace ui {
class ContextFactory;
}

namespace mojo {
namespace examples {

class GLES2ClientImpl;

class WindowTreeHostMojo : public aura::WindowTreeHost,
                           public NativeViewportClient {
 public:
  WindowTreeHostMojo(ScopedMessagePipeHandle viewport_handle,
                     const gfx::Rect& bounds,
                     const base::Callback<void()>& compositor_created_callback);
  virtual ~WindowTreeHostMojo();

  GLES2ClientImpl* gles2_client() { return gles2_client_.get(); }

 private:
  // WindowTreeHost:
  virtual aura::RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual void SetInsets(const gfx::Insets& insets) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void OnCursorVisibilityChanged(bool show) OVERRIDE;
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void PrepareForShutdown() OVERRIDE;

  // Overridden from NativeViewportClient:
  virtual void OnCreated() OVERRIDE;
  virtual void OnDestroyed() OVERRIDE;
  virtual void OnBoundsChanged(const Rect& bounds) OVERRIDE;
  virtual void OnEvent(const Event& event) OVERRIDE;

  void DidCreateContext(gfx::Size size);

  static ui::ContextFactory* context_factory_;

  scoped_ptr<GLES2ClientImpl> gles2_client_;
  RemotePtr<NativeViewport> native_viewport_;
  base::Callback<void()> compositor_created_callback_;

  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostMojo);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_AURA_DEMO_ROOT_WINDOW_HOST_MOJO_H_
