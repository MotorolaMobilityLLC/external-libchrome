// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_

#include <vector>

#include "mojo/services/public/cpp/view_manager/view_manager_types.h"

namespace mojo {
class Application;
namespace view_manager {

class View;
class ViewManagerDelegate;
class ViewTreeNode;

class ViewManager {
 public:
  // Delegate is owned by the caller.
  static void Create(Application* application, ViewManagerDelegate* delegate);

  // Returns all root nodes known to this connection.
  virtual const std::vector<ViewTreeNode*>& GetRoots() const = 0;

  // Returns a Node or View known to this connection.
  virtual ViewTreeNode* GetNodeById(Id id) = 0;
  virtual View* GetViewById(Id id) = 0;

 protected:
  virtual ~ViewManager() {}

};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_VIEW_MANAGER_H_
