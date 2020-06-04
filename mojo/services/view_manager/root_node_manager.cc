// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_node_manager.h"

#include "base/logging.h"
#include "mojo/services/view_manager/view_manager_connection.h"
#include "ui/aura/env.h"

namespace mojo {
namespace view_manager {
namespace service {
namespace {

// Id for the root node.
const TransportConnectionSpecificNodeId kRootId = 1;

// Used to identify an invalid connection.
const TransportConnectionId kNoConnection = 0;

}  // namespace

RootNodeManager::ScopedChange::ScopedChange(
    ViewManagerConnection* connection,
    RootNodeManager* root,
    RootNodeManager::ChangeType change_type)
    : root_(root),
      change_type_(change_type) {
  root_->PrepareForChange(connection);
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
      change_source_(kNoConnection),
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
  // TODO(sky): make a macro for this.
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    if (ShouldNotifyConnection(i->first)) {
      i->second->NotifyNodeHierarchyChanged(
          node, new_parent, old_parent, next_server_change_id_);
    }
  }
}

void RootNodeManager::NotifyNodeViewReplaced(const NodeId& node,
                                             const ViewId& new_view_id,
                                             const ViewId& old_view_id) {
  // TODO(sky): make a macro for this.
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    if (ShouldNotifyConnection(i->first))
      i->second->NotifyNodeViewReplaced(node, new_view_id, old_view_id);
  }
}

void RootNodeManager::NotifyNodeDeleted(const NodeId& node) {
  // TODO(sky): make a macro for this.
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    if (ShouldNotifyConnection(i->first))
      i->second->NotifyNodeDeleted(node, next_server_change_id_);
  }
}

void RootNodeManager::NotifyViewDeleted(const ViewId& view) {
  // TODO(sky): make a macro for this.
  for (ConnectionMap::iterator i = connection_map_.begin();
       i != connection_map_.end(); ++i) {
    if (ShouldNotifyConnection(i->first))
      i->second->NotifyViewDeleted(view);
  }
}

void RootNodeManager::PrepareForChange(ViewManagerConnection* connection) {
  // Should only ever have one change in flight.
  DCHECK_EQ(kNoConnection, change_source_);
  change_source_ = connection->id();
}

void RootNodeManager::FinishChange(ChangeType change_type) {
  // PrepareForChange/FinishChange should be balanced.
  DCHECK_NE(kNoConnection, change_source_);
  change_source_ = 0;
  if (change_type == CHANGE_TYPE_ADVANCE_SERVER_CHANGE_ID)
    next_server_change_id_++;
}

bool RootNodeManager::ShouldNotifyConnection(
    TransportConnectionId connection_id) const {
  // Don't notify the source that originated the change.
  return connection_id != change_source_;
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

}  // namespace service
}  // namespace view_manager
}  // namespace mojo
