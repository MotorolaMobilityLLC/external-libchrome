// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_node_manager.h"

#include "base/logging.h"
#include "mojo/services/view_manager/view_manager_connection.h"
#include "ui/aura/env.h"

namespace mojo {
namespace services {
namespace view_manager {
namespace {

// Id for the root node.
const TransportConnectionSpecificNodeId kRootId = 1;

}  // namespace

RootNodeManager::ScopedChange::ScopedChange(
    ViewManagerConnection* connection,
    RootNodeManager* root,
    TransportChangeId change_id,
    RootNodeManager::ChangeType change_type)
    : root_(root),
      change_type_(change_type) {
  root_->PrepareForChange(connection, change_id);
}

RootNodeManager::ScopedChange::~ScopedChange() {
  root_->FinishChange(change_type_);
}

RootNodeManager::Context::Context() {
  // Pass in false as native viewport creates the PlatformEventSource.
  aura::Env::CreateInstance(false);
}

RootNodeManager::Context::~Context() {
}

RootNodeManager::RootNodeManager(Shell* shell)
    : next_connection_id_(1),
      next_server_change_id_(1),
      root_view_manager_(shell, this),
      root_(this, NodeId(0, kRootId)) {
}

RootNodeManager::~RootNodeManager() {
  // All the connections should have been destroyed.
  DCHECK(connection_map_.empty());
}

TransportConnectionId RootNodeManager::GetAndAdvanceNextConnectionId() {
  const TransportConnectionId id = next_connection_id_++;
  DCHECK_LT(id, next_connection_id_);
  return id;
}

void RootNodeManager::AddConnection(ViewManagerConnection* connection) {
  DCHECK_EQ(0u, connection_map_.count(connection->id()));
  connection_map_[connection->id()] = connection;
}

void RootNodeManager::RemoveConnection(ViewManagerConnection* connection) {
  connection_map_.erase(connection->id());
}

ViewManagerConnection* RootNodeManager::GetConnection(
    TransportConnectionId connection_id) {
  ConnectionMap::iterator i = connection_map_.find(connection_id);
  return i == connection_map_.end() ? NULL : i->second;
}

Node* RootNodeManager::GetNode(const NodeId& id) {
  if (id == root_.id())
    return &root_;
  ConnectionMap::iterator i = connection_map_.find(id.connection_id);
  return i == connection_map_.end() ? NULL : i->second->GetNode(id);
}

View* RootNodeManager::GetView(const ViewId& id) {
  ConnectionMap::iterator i = connection_map_.find(id.connection_id);
  return i == connection_map_.end() ? NULL : i->second->GetView(id);
}

void RootNodeManager::NotifyNodeHierarchyChanged(const NodeId& node,
                                                 const NodeId& new_parent,
                                                 const NodeId& old_parent) {
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    i->second->NotifyNodeHierarchyChanged(
        node, new_parent, old_parent, next_server_change_id_,
        GetClientChangeId(i->first));
  }
}

void RootNodeManager::NotifyNodeViewReplaced(const NodeId& node,
                                             const ViewId& new_view_id,
                                             const ViewId& old_view_id) {
  // TODO(sky): make a macro for this.
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    i->second->NotifyNodeViewReplaced(node, new_view_id, old_view_id,
                                      GetClientChangeId(i->first));
  }
}

void RootNodeManager::NotifyNodeDeleted(const NodeId& node) {
  // TODO(sky): make a macro for this.
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    i->second->NotifyNodeDeleted(node, next_server_change_id_,
                                 GetClientChangeId(i->first));
  }
}

void RootNodeManager::PrepareForChange(ViewManagerConnection* connection,
                                       TransportChangeId change_id) {
  DCHECK(!change_.get());  // Should only ever have one change in flight.
  change_.reset(new Change(connection->id(), change_id));
}

void RootNodeManager::FinishChange(ChangeType change_type) {
  DCHECK(change_.get());  // PrepareForChange/FinishChange should be balanced.
  change_.reset();
  if (change_type == CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID)
    next_server_change_id_++;
}

TransportChangeId RootNodeManager::GetClientChangeId(
    TransportConnectionId connection_id) const {
  return (change_ && connection_id == change_->connection_id) ?
      change_->client_change_id : 0;
}

void RootNodeManager::OnNodeHierarchyChanged(const NodeId& node,
                                             const NodeId& new_parent,
                                             const NodeId& old_parent) {
  if (!root_view_manager_.in_setup())
    NotifyNodeHierarchyChanged(node, new_parent, old_parent);
}

void RootNodeManager::OnNodeViewReplaced(const NodeId& node,
                                         const ViewId& new_view_id,
                                         const ViewId& old_view_id) {
  NotifyNodeViewReplaced(node, new_view_id, old_view_id);
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
