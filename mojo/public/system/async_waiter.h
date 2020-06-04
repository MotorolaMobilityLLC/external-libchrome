// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_SYSTEM_ASYNC_WAITER_H_
#define MOJO_PUBLIC_SYSTEM_ASYNC_WAITER_H_

#include "mojo/public/system/core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uintptr_t MojoAsyncWaitID;

typedef void (*MojoAsyncWaitCallback)(void* closure, MojoResult result);

struct MojoAsyncWaiter {
  // Asynchronously call MojoWait on a background thread, and pass the result
  // of MojoWait to the given MojoAsyncWaitCallback on the current thread.
  // Returns a non-zero MojoAsyncWaitID that can be used with CancelWait to
  // stop waiting. This identifier becomes invalid once the callback runs.
  MojoAsyncWaitID (*AsyncWait)(struct MojoAsyncWaiter* waiter,
                               MojoHandle handle,
                               MojoWaitFlags flags,
                               MojoDeadline deadline,
                               MojoAsyncWaitCallback callback,
                               void* closure);

  // Cancel an existing call to AsyncWait with the given MojoAsyncWaitID. The
  // corresponding MojoAsyncWaitCallback will not be called in this case.
  void (*CancelWait)(struct MojoAsyncWaiter* waiter,
                     MojoAsyncWaitID wait_id);
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_SYSTEM_ASYNC_WAITER_H_
