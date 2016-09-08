// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BIND_H_
#define BASE_BIND_H_

#include "base/bind_internal.h"

// -----------------------------------------------------------------------------
// Usage documentation
// -----------------------------------------------------------------------------
//
// See //docs/callback.md for documentation.
//
//
// -----------------------------------------------------------------------------
// Implementation notes
// -----------------------------------------------------------------------------
//
// If you're reading the implementation, before proceeding further, you should
// read the top comment of base/bind_internal.h for a definition of common
// terms and concepts.

namespace base {

template <typename Functor, typename... Args>
inline base::Callback<MakeUnboundRunType<Functor, Args...>> Bind(
    Functor&& functor,
    Args&&... args) {
  using BindState = internal::MakeBindStateType<Functor, Args...>;
  using UnboundRunType = MakeUnboundRunType<Functor, Args...>;
  using Invoker = internal::Invoker<BindState, UnboundRunType>;
  using CallbackType = Callback<UnboundRunType>;

  // Store the invoke func into PolymorphicInvoke before casting it to
  // InvokeFuncStorage, so that we can ensure its type matches to
  // PolymorphicInvoke, to which CallbackType will cast back.
  using PolymorphicInvoke = typename CallbackType::PolymorphicInvoke;
  PolymorphicInvoke invoke_func = &Invoker::Run;

  using InvokeFuncStorage = internal::BindStateBase::InvokeFuncStorage;
  return CallbackType(new BindState(
      reinterpret_cast<InvokeFuncStorage>(invoke_func),
      std::forward<Functor>(functor),
      std::forward<Args>(args)...));
}

}  // namespace base

#endif  // BASE_BIND_H_
