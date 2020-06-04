// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_NODE_H_
#define MOJO_SERVICES_VIEW_MANAGER_NODE_H_

#include <vector>

#include "base/logging.h"
#include "mojo/services/view_manager/ids.h"
#include "mojo/services/view_manager/view_manager_export.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_observer.h"

namespace mojo {
namespace services {
namespace view_manager {

class NodeDelegate;
namespace service {
class View;
}

// Represents a node in the graph. Delegate is informed of interesting events.
class MOJO_VIEW_MANAGER_EXPORT Node
    : public aura::WindowObserver,
      public aura::WindowDelegate {
 public:
  Node(NodeDelegate* delegate, const NodeId& id);
  virtual ~Node();

  void set_view_id(const ViewId& view_id) { view_id_ = view_id; }
  const ViewId& view_id() const { return view_id_; }

  const NodeId& id() const { return id_; }

  void Add(Node* child);
  void Remove(Node* child);

  aura::Window* window() { return &window_; }

  Node* GetParent();

  std::vector<Node*> GetChildren();

  // Sets the view associated with this node. Node does not own its View.
  void SetView(service::View* view);
  service::View* view() { return view_; }

 private:
  // WindowObserver overrides:
  virtual void OnWindowHierarchyChanged(
      const aura::WindowObserver::HierarchyChangeParams& params) OVERRIDE;

  // WindowDelegate overrides:
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual gfx::Size GetMaximumSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds,
                               const gfx::Rect& new_bounds) OVERRIDE;
  virtual gfx::NativeCursor GetCursor(const gfx::Point& point) OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) OVERRIDE;
  virtual bool CanFocus() OVERRIDE;
  virtual void OnCaptureLost() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;
  virtual void OnWindowTargetVisibilityChanged(bool visible) OVERRIDE;
  virtual bool HasHitTestMask() const OVERRIDE;
  virtual void GetHitTestMask(gfx::Path* mask) const OVERRIDE;

  NodeDelegate* delegate_;
  const NodeId id_;

  // Weak pointer to view associated with this node.
  service::View* view_;

  ViewId view_id_;

  aura::Window window_;

  DISALLOW_COPY_AND_ASSIGN(Node);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_NODE_H_
