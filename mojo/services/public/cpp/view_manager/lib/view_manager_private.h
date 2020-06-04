// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_

#include "base/basictypes.h"
#include "mojo/services/public/cpp/view_manager/lib/view_manager_synchronizer.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"

namespace mojo {
namespace view_manager {

class ViewManagerSynchronizer;

class ViewManagerPrivate {
 public:
  explicit ViewManagerPrivate(ViewManager* manager);
  ~ViewManagerPrivate();

  ViewManagerSynchronizer* synchronizer() {
    return manager_->synchronizer_;
  }
  void set_synchronizer(ViewManagerSynchronizer* synchronizer) {
    manager_->synchronizer_ = synchronizer;
  }

  void AddRoot(ViewTreeNode* root);
  void RemoveRoot(ViewTreeNode* root);

  void AddNode(Id node_id, ViewTreeNode* node);
  void RemoveNode(Id node_id);

  void AddView(Id view_id, View* view);
  void RemoveView(Id view_id);

  // Returns true if the ViewManager's synchronizer is connected to the service.
  bool connected() { return manager_->synchronizer_->connected(); }

 private:
  ViewManager* manager_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerPrivate);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_PRIVATE_H_
