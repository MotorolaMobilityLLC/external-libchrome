// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/shell/service.h"

namespace mojo {
namespace internal {

ServiceConnectorBase::Owner::Owner(ScopedShellHandle shell_handle)
  : shell_(shell_handle.Pass(), this) {
}

ServiceConnectorBase::Owner::~Owner() {}

ServiceConnectorBase::~ServiceConnectorBase() {}

}  // namespace internal
}  // namespace mojo
