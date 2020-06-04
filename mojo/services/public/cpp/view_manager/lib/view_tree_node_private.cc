// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/public/cpp/view_manager/lib/view_tree_node_private.h"

namespace mojo {
namespace view_manager {

ViewTreeNodePrivate::ViewTreeNodePrivate(ViewTreeNode* node)
    : node_(node) {
}

ViewTreeNodePrivate::~ViewTreeNodePrivate() {
}

// static
ViewTreeNode* ViewTreeNodePrivate::LocalCreate() {
  return new ViewTreeNode;
}

}  // namespace view_manager
}  // namespace mojo
