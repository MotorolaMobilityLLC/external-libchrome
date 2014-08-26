// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_
#define MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace gfx {
class Rect;
}

namespace aura {
namespace client {
class FocusClient;
class WindowTreeClient;
}
class Window;
class WindowTreeHost;
}

namespace gfx {
class Screen;
}

namespace mojo {

class ApplicationConnection;

namespace service {

class Node;
class RootNodeManager;
class RootViewManagerDelegate;

// RootViewManager binds the root node to an actual display.
class MOJO_VIEW_MANAGER_EXPORT RootViewManager {
 public:
  RootViewManager(ApplicationConnection* app_connection,
                  RootNodeManager* root_node,
                  RootViewManagerDelegate* delegate,
                  const Callback<void()>& native_viewport_closed_callback);
  virtual ~RootViewManager();

  // Schedules a paint for the specified region of the specified node.
  void SchedulePaint(const Node* node, const gfx::Rect& bounds);

  // See description above field for details.
  bool in_setup() const { return in_setup_; }

 private:
  class RootWindowDelegateImpl;

  void OnCompositorCreated();

  RootViewManagerDelegate* delegate_;

  RootNodeManager* root_node_manager_;

  // Returns true if adding the root node's window to |window_tree_host_|.
  bool in_setup_;

  scoped_ptr<RootWindowDelegateImpl> window_delegate_;

  // Owned by its parent aura::Window.
  aura::Window* root_window_;

  scoped_ptr<gfx::Screen> screen_;
  scoped_ptr<aura::WindowTreeHost> window_tree_host_;
  scoped_ptr<aura::client::WindowTreeClient> window_tree_client_;
  scoped_ptr<aura::client::FocusClient> focus_client_;

  DISALLOW_COPY_AND_ASSIGN(RootViewManager);
};

}  // namespace service
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_ROOT_VIEW_MANAGER_H_
