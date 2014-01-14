// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_ENVIRONMENT_DEFAULT_ASYNC_WAITER_IMPL_H_
#define MOJO_ENVIRONMENT_DEFAULT_ASYNC_WAITER_IMPL_H_

#include "mojo/environment/mojo_environment_impl_export.h"
#include "mojo/public/system/async_waiter.h"

namespace mojo {
namespace internal {

MOJO_ENVIRONMENT_IMPL_EXPORT MojoAsyncWaiter* GetDefaultAsyncWaiterImpl();

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_ENVIRONMENT_DEFAULT_ASYNC_WAITER_IMPL_H_
