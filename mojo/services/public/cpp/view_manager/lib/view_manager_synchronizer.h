// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_
#define MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"
#include "mojo/services/public/interfaces/view_manager/view_manager.mojom.h"

class SkBitmap;

namespace mojo {
namespace view_manager {

class ViewManager;
class ViewManagerTransaction;

// Manages the connection with the View Manager service.
class ViewManagerSynchronizer : public InterfaceImpl<IViewManagerClient> {
 public:
  explicit ViewManagerSynchronizer(ViewManagerDelegate* delegate);
  virtual ~ViewManagerSynchronizer();

  bool connected() const { return connected_; }
  TransportConnectionId connection_id() const { return connection_id_; }

  // API exposed to the node/view implementations that pushes local changes to
  // the service.
  TransportNodeId CreateViewTreeNode();
  void DestroyViewTreeNode(TransportNodeId node_id);

  TransportViewId CreateView();
  void DestroyView(TransportViewId view_id);

  // These methods take TransportIds. For views owned by the current connection,
  // the connection id high word can be zero. In all cases, the TransportId 0x1
  // refers to the root node.
  void AddChild(TransportNodeId child_id, TransportNodeId parent_id);
  void RemoveChild(TransportNodeId child_id, TransportNodeId parent_id);

  bool OwnsNode(TransportNodeId id) const;
  bool OwnsView(TransportViewId id) const;

  void SetActiveView(TransportNodeId node_id, TransportViewId view_id);
  void SetBounds(TransportNodeId node_id, const gfx::Rect& bounds);
  void SetViewContents(TransportViewId view_id, const SkBitmap& contents);

  void Embed(const String& url, TransportNodeId node_id);

  void set_changes_acked_callback(const base::Callback<void(void)>& callback) {
    changes_acked_callback_ = callback;
  }
  void ClearChangesAckedCallback() {
    changes_acked_callback_ = base::Callback<void(void)>();
  }

 private:
  friend class ViewManagerTransaction;
  typedef ScopedVector<ViewManagerTransaction> Transactions;

  // Overridden from InterfaceImpl:
  virtual void OnConnectionEstablished() OVERRIDE;

  // Overridden from IViewManagerClient:
  virtual void OnViewManagerConnectionEstablished(
      TransportConnectionId connection_id,
      TransportChangeId next_server_change_id,
      mojo::Array<INodePtr> nodes) OVERRIDE;
  virtual void OnRootsAdded(Array<INodePtr> nodes) OVERRIDE;
  virtual void OnServerChangeIdAdvanced(
      uint32_t next_server_change_id) OVERRIDE;
  virtual void OnNodeBoundsChanged(uint32 node_id,
                                   RectPtr old_bounds,
                                   RectPtr new_bounds) OVERRIDE;
  virtual void OnNodeHierarchyChanged(
      uint32 node_id,
      uint32 new_parent_id,
      uint32 old_parent_id,
      TransportChangeId server_change_id,
      mojo::Array<INodePtr> nodes) OVERRIDE;
  virtual void OnNodeDeleted(TransportNodeId node_id,
                             TransportChangeId server_change_id) OVERRIDE;
  virtual void OnNodeViewReplaced(uint32_t node,
                                  uint32_t new_view_id,
                                  uint32_t old_view_id) OVERRIDE;
  virtual void OnViewDeleted(uint32_t view_id) OVERRIDE;
  virtual void OnViewInputEvent(uint32_t view,
                                EventPtr event,
                                const Callback<void()>& callback) OVERRIDE;

  // Sync the client model with the service by enumerating the pending
  // transaction queue and applying them in order.
  void Sync();

  // Removes |transaction| from the pending queue. |transaction| must be at the
  // front of the queue.
  void RemoveFromPendingQueue(ViewManagerTransaction* transaction);

  ViewManager* view_manager() { return view_manager_.get(); }

  scoped_ptr<ViewManager> view_manager_;
  bool connected_;
  TransportConnectionId connection_id_;
  uint16_t next_id_;
  TransportChangeId next_server_change_id_;

  Transactions pending_transactions_;

  base::WeakPtrFactory<ViewManagerSynchronizer> sync_factory_;

  base::Callback<void(void)> changes_acked_callback_;

  IViewManager* service_;

  DISALLOW_COPY_AND_ASSIGN(ViewManagerSynchronizer);
};

}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_PUBLIC_CPP_VIEW_MANAGER_LIB_VIEW_MANAGER_SYNCHRONIZER_H_
