// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "mojo/examples/window_manager/window_manager.mojom.h"
#include "mojo/public/cpp/application/application.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_delegate.h"
#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node.h"
#include "mojo/services/public/cpp/view_manager/view_tree_node_observer.h"
#include "ui/events/event_constants.h"

using mojo::view_manager::View;
using mojo::view_manager::ViewManager;
using mojo::view_manager::ViewManagerDelegate;
using mojo::view_manager::ViewObserver;
using mojo::view_manager::ViewTreeNode;
using mojo::view_manager::ViewTreeNodeObserver;

namespace mojo {
namespace examples {
namespace {

const SkColor kColors[] = { SK_ColorYELLOW,
                            SK_ColorRED,
                            SK_ColorGREEN,
                            SK_ColorMAGENTA };

}  // namespace

class EmbeddedApp : public Application,
                    public ViewManagerDelegate,
                    public ViewObserver,
                    public ViewTreeNodeObserver {
 public:
  EmbeddedApp() {}
  virtual ~EmbeddedApp() {}

 private:
  // Overridden from Application:
  virtual void Initialize() MOJO_OVERRIDE {
    ViewManager::Create(this, this);
    ConnectTo<IWindowManager>("mojo:mojo_window_manager", &window_manager_);
  }

  // Overridden from ViewManagerDelegate:
  virtual void OnRootAdded(ViewManager* view_manager,
                           ViewTreeNode* root) OVERRIDE {
    View* view = View::Create(view_manager);
    view->AddObserver(this);
    root->SetActiveView(view);
    root->AddObserver(this);
    size_t index = view_manager->GetRoots().size() - 1;
    view->SetColor(kColors[index % arraysize(kColors)]);
  }
  virtual void OnRootRemoved(ViewManager* view_manager,
                             ViewTreeNode* root) OVERRIDE {
    std::map<ViewTreeNode*, View*>::const_iterator it =
        views_to_reap_.find(root);
    if (it != views_to_reap_.end())
      it->second->Destroy();
  }

  // Overridden from ViewObserver:
  virtual void OnViewInputEvent(View* view, EventPtr event) OVERRIDE {
    if (event->action == ui::ET_MOUSE_RELEASED)
      window_manager_->CloseWindow(view->node()->id());
  }

  // Overridden from ViewTreeNodeObserver:
  virtual void OnNodeActiveViewChange(
      ViewTreeNode* node,
      View* old_view,
      View* new_view,
      ViewTreeNodeObserver::DispositionChangePhase phase) OVERRIDE {
    if (new_view == 0)
      views_to_reap_[node] = old_view;
  }

  IWindowManagerPtr window_manager_;
  std::map<ViewTreeNode*, View*> views_to_reap_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedApp);
};

}  // namespace examples

// static
Application* Application::Create() {
  return new examples::EmbeddedApp;
}

}  // namespace mojo