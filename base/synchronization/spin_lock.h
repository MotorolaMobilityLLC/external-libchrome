// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SYNCHRONIZATION_SPIN_LOCK_H
#define BASE_SYNCHRONIZATION_SPIN_LOCK_H

#include <atomic>
#include <memory>
#include <mutex>

#include "base/base_export.h"
#include "base/compiler_specific.h"

// Spinlock is a simple spinlock class based on the standard CPU primitive of
// atomic increment and decrement of an int at a given memory address. These are
// intended only for very short duration locks and assume a system with multiple
// cores. For any potentially longer wait you should use a real lock, such as
// |base::Lock|.
//
// |SpinLock|s MUST be globals. Using them as (e.g.) struct/class members will
// result in an uninitialized lock, which is dangerously incorrect.

namespace base {
namespace subtle {

class SpinLock {
 public:
  using Guard = std::lock_guard<SpinLock>;

  ALWAYS_INLINE void lock() {
    static_assert(sizeof(lock_) == sizeof(int),
                  "int and lock_ are different sizes");
    if (LIKELY(!lock_.exchange(true, std::memory_order_acquire)))
      return;
    LockSlow();
  }

  ALWAYS_INLINE void unlock() { lock_.store(false, std::memory_order_release); }

 private:
  // This is called if the initial attempt to acquire the lock fails. It's
  // slower, but has a much better scheduling and power consumption behavior.
  BASE_EXPORT void LockSlow();

  std::atomic_int lock_;
};

}  // namespace subtle
}  // namespace base

#endif  // BASE_SYNCHRONIZATION_SPIN_LOCK_H
