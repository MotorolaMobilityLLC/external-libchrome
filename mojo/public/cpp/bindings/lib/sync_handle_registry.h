// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_SYNC_HANDLE_REGISTRY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_SYNC_HANDLE_REGISTRY_H_

#include <unordered_map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/system/core.h"

namespace mojo {
namespace internal {

// SyncHandleRegistry is a thread-local storage to register handles that want to
// be watched together.
//
// This class is not thread safe.
class SyncHandleRegistry : public base::MessageLoop::DestructionObserver {
 public:
  // Returns a thread-local object.
  static SyncHandleRegistry* current();

  using HandleCallback = base::Callback<void(MojoResult)>;
  bool RegisterHandle(const Handle& handle,
                      MojoHandleSignals handle_signals,
                      const HandleCallback& callback);

  void UnregisterHandle(const Handle& handle);

  // Waits on all the registered handles and runs callbacks synchronously for
  // those ready handles.
  // The method:
  //   - returns true when any element of |should_stop| is set to true;
  //   - returns false when any error occurs.
  bool WatchAllHandles(const bool* should_stop[], size_t count);

 private:
  struct HandleHasher {
    size_t operator()(const Handle& handle) const {
      return std::hash<uint32_t>()(static_cast<uint32_t>(handle.value()));
    }
  };
  using HandleMap = std::unordered_map<Handle, HandleCallback, HandleHasher>;

  SyncHandleRegistry();
  ~SyncHandleRegistry() override;

  // base::MessageLoop::DestructionObserver implementation:
  void WillDestroyCurrentMessageLoop() override;

  HandleMap handles_;

  ScopedHandle wait_set_handle_;

  scoped_refptr<base::RefCountedData<bool>> destroyed_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SyncHandleRegistry);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_SYNC_HANDLE_REGISTRY_H_
