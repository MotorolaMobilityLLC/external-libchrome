// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE2_IMPL_H_
#define MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE2_IMPL_H_

#include "base/basictypes.h"
#include "mojo/services/public/cpp/view_manager/types.h"
#include "mojo/services/public/interfaces/window_manager2/window_manager2.mojom.h"

namespace mojo {

class WindowManagerApp;

class WindowManagerService2Impl : public InterfaceImpl<WindowManagerService2> {
 public:
  explicit WindowManagerService2Impl(WindowManagerApp* manager);
  virtual ~WindowManagerService2Impl();

  void NotifyReady();
  void NotifyViewFocused(Id new_focused_id, Id old_focused_id);
  void NotifyWindowActivated(Id new_active_id, Id old_active_id);

 private:
  // Overridden from WindowManagerService:
  virtual void SetCapture(Id view,
                          const Callback<void(bool)>& callback) override;
  virtual void FocusWindow(Id view,
                           const Callback<void(bool)>& callback) override;
  virtual void ActivateWindow(Id view,
                              const Callback<void(bool)>& callback) override;

  // Overridden from InterfaceImpl:
  virtual void OnConnectionEstablished() override;

  WindowManagerApp* window_manager_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerService2Impl);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_WINDOW_MANAGER_WINDOW_MANAGER_SERVICE2_IMPL_H_
