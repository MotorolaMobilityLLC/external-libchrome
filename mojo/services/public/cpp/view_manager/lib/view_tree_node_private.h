// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_TREE_NODE_PRIVATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_TREE_NODE_PRIVATE_H_

#include <vector>

#include "base/basictypes.h"

#include "mojo/services/public/cpp/view_manager/view_tree_node.h"

namespace mojo {
namespace services {
namespace view_manager {

class ViewTreeNodePrivate {
 public:
  explicit ViewTreeNodePrivate(ViewTreeNode* node);
  ~ViewTreeNodePrivate();

  static ViewTreeNode* LocalCreate();

  ObserverList<ViewTreeNodeObserver>* observers() { return &node_->observers_; }

  void ClearParent() { node_->parent_ = NULL; }

  void set_id(TransportNodeId id) { node_->id_ = id; }

  ViewManager* view_manager() { return node_->manager_; }
  void set_view_manager(ViewManager* manager) {
    node_->manager_ = manager;
  }

  void LocalDestroy() {
    node_->LocalDestroy();
  }
  void LocalAddChild(ViewTreeNode* child) {
    node_->LocalAddChild(child);
  }
  void LocalRemoveChild(ViewTreeNode* child) {
    node_->LocalRemoveChild(child);
  }

 private:
  ViewTreeNode* node_;

  DISALLOW_COPY_AND_ASSIGN(ViewTreeNodePrivate);
};

}  // namespace view_manager
}  // namespace services
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_TREE_NODE_PRIVATE_H_
