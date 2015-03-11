// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/logging.h"
#include "base/memory/discardable_memory_shmem.h"

namespace base {

// static
void DiscardableMemory::GetSupportedTypes(
    std::vector<DiscardableMemoryType>* types) {
  const DiscardableMemoryType supported_types[] = {
    DISCARDABLE_MEMORY_TYPE_SHMEM
  };
  types->assign(supported_types, supported_types + arraysize(supported_types));
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemoryWithType(
    DiscardableMemoryType type, size_t size) {
  switch (type) {
    case DISCARDABLE_MEMORY_TYPE_SHMEM: {
      scoped_ptr<internal::DiscardableMemoryShmem> memory(
          new internal::DiscardableMemoryShmem(size));
      if (!memory->Initialize())
        return nullptr;

      return memory.Pass();
    }
    case DISCARDABLE_MEMORY_TYPE_NONE:
      NOTREACHED();
      return nullptr;
  }

  NOTREACHED();
  return nullptr;
}

}  // namespace base
