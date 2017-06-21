// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_tracker.h"

#include "base/memory/shared_memory.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/memory_allocator_dump_guid.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"

namespace base {

// static
SharedMemoryTracker* SharedMemoryTracker::GetInstance() {
  static SharedMemoryTracker* instance = new SharedMemoryTracker;
  return instance;
}

// static
std::string SharedMemoryTracker::GetDumpNameForTracing(
    const UnguessableToken& id) {
  return "shared_memory/" + id.ToString();
}

// static
trace_event::MemoryAllocatorDumpGuid
SharedMemoryTracker::GetGlobalDumpGUIDForTracing(const UnguessableToken& id) {
  std::string dump_name = GetDumpNameForTracing(id);
  return trace_event::MemoryAllocatorDumpGuid(dump_name);
}

void SharedMemoryTracker::IncrementMemoryUsage(
    const SharedMemory& shared_memory) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(&shared_memory) == usages_.end());
  usages_[&shared_memory] = shared_memory.mapped_size();
}

void SharedMemoryTracker::DecrementMemoryUsage(
    const SharedMemory& shared_memory) {
  AutoLock hold(usages_lock_);
  DCHECK(usages_.find(&shared_memory) != usages_.end());
  usages_.erase(&shared_memory);
}

bool SharedMemoryTracker::OnMemoryDump(const trace_event::MemoryDumpArgs& args,
                                       trace_event::ProcessMemoryDump* pmd) {
  std::vector<std::tuple<UnguessableToken, uintptr_t, size_t>> usages;
  {
    AutoLock hold(usages_lock_);
    usages.reserve(usages_.size());
    for (const auto& usage : usages_) {
      usages.emplace_back(usage.first->handle().GetGUID(),
                          reinterpret_cast<uintptr_t>(usage.first->memory()),
                          usage.second);
    }
  }
  for (const auto& usage : usages) {
    const UnguessableToken& memory_guid = std::get<0>(usage);
    uintptr_t address = std::get<1>(usage);
    size_t size = std::get<2>(usage);
    std::string dump_name;
    if (memory_guid.is_empty()) {
      // TODO(hajimehoshi): As passing ID across mojo is not implemented yet
      // (crbug/713763), ID can be empty. For such case, use an address instead
      // of GUID so that approximate memory usages are available.
      dump_name = "shared_memory/" + Uint64ToString(address);
    } else {
      dump_name = GetDumpNameForTracing(memory_guid);
    }
    // Discard duplicates that might be seen in single-process mode.
    // TODO(hajimehoshi): This logic depends on the old fact that dumps were not
    // created nowhere but SharedMemoryTracker::OnMemoryDump. Now this is not
    // true since ProcessMemoryDump::CreateSharedMemoryOwnershipEdge creates
    // dumps, and the logic needs to be fixed. See the discussion at
    // https://chromium-review.googlesource.com/c/535378.
    if (pmd->GetAllocatorDump(dump_name))
      continue;
    trace_event::MemoryAllocatorDump* local_dump =
        pmd->CreateAllocatorDump(dump_name);
    // TODO(hajimehoshi): The size is not resident size but virtual size so far.
    // Fix this to record resident size.
    local_dump->AddScalar(trace_event::MemoryAllocatorDump::kNameSize,
                          trace_event::MemoryAllocatorDump::kUnitsBytes, size);
    auto global_dump_guid = GetGlobalDumpGUIDForTracing(memory_guid);
    trace_event::MemoryAllocatorDump* global_dump =
        pmd->CreateSharedGlobalAllocatorDump(global_dump_guid);
    global_dump->AddScalar(trace_event::MemoryAllocatorDump::kNameSize,
                           trace_event::MemoryAllocatorDump::kUnitsBytes, size);

    // The edges will be overriden by the clients with correct importance.
    pmd->AddOverridableOwnershipEdge(local_dump->guid(), global_dump->guid(),
                                     0 /* importance */);
  }
  return true;
}

SharedMemoryTracker::SharedMemoryTracker() {
  trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "SharedMemoryTracker", nullptr);
}

SharedMemoryTracker::~SharedMemoryTracker() = default;

}  // namespace
