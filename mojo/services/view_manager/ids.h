// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_IDS_H_
#define MOJO_SERVICES_VIEW_MANAGER_IDS_H_

#include "mojo/services/public/cpp/view_manager/util.h"
#include "mojo/services/public/cpp/view_manager/view_manager_types.h"
#include "mojo/services/view_manager/view_manager_export.h"

namespace mojo {
namespace view_manager {
namespace service {

// Adds a bit of type safety to node ids.
struct MOJO_VIEW_MANAGER_EXPORT NodeId {
  NodeId(TransportConnectionId connection_id,
         TransportConnectionSpecificNodeId node_id)
      : connection_id(connection_id),
        node_id(node_id) {}
  NodeId() : connection_id(0), node_id(0) {}

  bool operator==(const NodeId& other) const {
    return other.connection_id == connection_id &&
        other.node_id == node_id;
  }

  bool operator!=(const NodeId& other) const {
    return !(*this == other);
  }

  TransportConnectionId connection_id;
  TransportConnectionSpecificNodeId node_id;
};

// Adds a bit of type safety to view ids.
struct MOJO_VIEW_MANAGER_EXPORT ViewId {
  ViewId(TransportConnectionId connection_id,
         TransportConnectionSpecificViewId view_id)
      : connection_id(connection_id),
        view_id(view_id) {}
  ViewId() : connection_id(0), view_id(0) {}

  bool operator==(const ViewId& other) const {
    return other.connection_id == connection_id &&
        other.view_id == view_id;
  }

  bool operator!=(const ViewId& other) const {
    return !(*this == other);
  }

  TransportConnectionId connection_id;
  TransportConnectionSpecificViewId view_id;
};

// Functions for converting to/from structs and transport values.
inline NodeId NodeIdFromTransportId(TransportNodeId id) {
  return NodeId(HiWord(id), LoWord(id));
}

inline TransportNodeId NodeIdToTransportId(const NodeId& id) {
  return (id.connection_id << 16) | id.node_id;
}

inline ViewId ViewIdFromTransportId(TransportViewId id) {
  return ViewId(HiWord(id), LoWord(id));
}

inline TransportViewId ViewIdToTransportId(const ViewId& id) {
  return (id.connection_id << 16) | id.view_id;
}

}  // namespace service
}  // namespace view_manager
}  // namespace mojo

#endif  // MOJO_SERVICES_VIEW_MANAGER_IDS_H_
