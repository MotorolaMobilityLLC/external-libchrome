// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_shared_memory.h"

#include <stdint.h>

#include <algorithm>

#include "base/atomicops.h"
#include "base/bits.h"
#include "base/logging.h"
#include "base/memory/shared_memory_tracker.h"
#include "base/numerics/safe_math.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
// For madvise() which is available on all POSIX compatible systems.
#include <sys/mman.h>
#endif

#if defined(OS_ANDROID)
#include "third_party/ashmem/ashmem.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

namespace base {
namespace {

// Use a machine-sized pointer as atomic type. It will use the Atomic32 or
// Atomic64 routines, depending on the architecture.
typedef intptr_t AtomicType;
typedef uintptr_t UAtomicType;

// Template specialization for timestamp serialization/deserialization. This
// is used to serialize timestamps using Unix time on systems where AtomicType
// does not have enough precision to contain a timestamp in the standard
// serialized format.
template <int>
Time TimeFromWireFormat(int64_t value);
template <int>
int64_t TimeToWireFormat(Time time);

// Serialize to Unix time when using 4-byte wire format.
// Note: 19 January 2038, this will cease to work.
template <>
Time ALLOW_UNUSED_TYPE TimeFromWireFormat<4>(int64_t value) {
  return value ? Time::UnixEpoch() + TimeDelta::FromSeconds(value) : Time();
}
template <>
int64_t ALLOW_UNUSED_TYPE TimeToWireFormat<4>(Time time) {
  return time > Time::UnixEpoch() ? (time - Time::UnixEpoch()).InSeconds() : 0;
}

// Standard serialization format when using 8-byte wire format.
template <>
Time ALLOW_UNUSED_TYPE TimeFromWireFormat<8>(int64_t value) {
  return Time::FromInternalValue(value);
}
template <>
int64_t ALLOW_UNUSED_TYPE TimeToWireFormat<8>(Time time) {
  return time.ToInternalValue();
}

struct SharedState {
  enum LockState { UNLOCKED = 0, LOCKED = 1 };

  explicit SharedState(AtomicType ivalue) { value.i = ivalue; }
  SharedState(LockState lock_state, Time timestamp) {
    int64_t wire_timestamp = TimeToWireFormat<sizeof(AtomicType)>(timestamp);
    DCHECK_GE(wire_timestamp, 0);
    DCHECK_EQ(lock_state & ~1, 0);
    value.u = (static_cast<UAtomicType>(wire_timestamp) << 1) | lock_state;
  }

  LockState GetLockState() const { return static_cast<LockState>(value.u & 1); }

  Time GetTimestamp() const {
    return TimeFromWireFormat<sizeof(AtomicType)>(value.u >> 1);
  }

  // Bit 1: Lock state. Bit is set when locked.
  // Bit 2..sizeof(AtomicType)*8: Usage timestamp. NULL time when locked or
  // purged.
  union {
    AtomicType i;
    UAtomicType u;
  } value;
};

// Shared state is stored at offset 0 in shared memory segments.
SharedState* SharedStateFromSharedMemory(const SharedMemory& shared_memory) {
  DCHECK(shared_memory.memory());
  return static_cast<SharedState*>(shared_memory.memory());
}

// Round up |size| to a multiple of page size.
size_t AlignToPageSize(size_t size) {
  return bits::Align(size, base::GetPageSize());
}

// LockPages/UnlockPages are platform-native discardable page management
// helper functions. Both expect |offset| to be specified relative to the
// base address at which |memory| is mapped, and that |offset| and |length|
// are page-aligned by the caller.

// Returns SUCCESS on platforms which do not support discardable pages.
DiscardableSharedMemory::LockResult LockPages(const SharedMemory& memory,
                                              size_t offset,
                                              size_t length) {
#if defined(OS_ANDROID)
  SharedMemoryHandle handle = memory.handle();
  if (handle.IsValid()) {
    int pin_result = ashmem_pin_region(handle.GetHandle(), offset, length);
    if (pin_result == ASHMEM_WAS_PURGED)
      return DiscardableSharedMemory::PURGED;
    if (pin_result < 0)
      return DiscardableSharedMemory::FAILED;
  }
#endif
  return DiscardableSharedMemory::SUCCESS;
}

// UnlockPages() is a no-op on platforms not supporting discardable pages.
void UnlockPages(const SharedMemory& memory, size_t offset, size_t length) {
#if defined(OS_ANDROID)
  SharedMemoryHandle handle = memory.handle();
  if (handle.IsValid()) {
    int unpin_result = ashmem_unpin_region(handle.GetHandle(), offset, length);
    DCHECK_EQ(0, unpin_result);
  }
#endif
}

}  // namespace

DiscardableSharedMemory::DiscardableSharedMemory()
    : mapped_size_(0), locked_page_count_(0) {
}

DiscardableSharedMemory::DiscardableSharedMemory(
    SharedMemoryHandle shared_memory_handle)
    : shared_memory_(shared_memory_handle, false),
      mapped_size_(0),
      locked_page_count_(0) {
}

DiscardableSharedMemory::~DiscardableSharedMemory() {
}

bool DiscardableSharedMemory::CreateAndMap(size_t size) {
  CheckedNumeric<size_t> checked_size = size;
  checked_size += AlignToPageSize(sizeof(SharedState));
  if (!checked_size.IsValid())
    return false;

  if (!shared_memory_.CreateAndMapAnonymous(checked_size.ValueOrDie()))
    return false;

  mapped_size_ =
      shared_memory_.mapped_size() - AlignToPageSize(sizeof(SharedState));

  locked_page_count_ = AlignToPageSize(mapped_size_) / base::GetPageSize();
#if DCHECK_IS_ON()
  for (size_t page = 0; page < locked_page_count_; ++page)
    locked_pages_.insert(page);
#endif

  DCHECK(last_known_usage_.is_null());
  SharedState new_state(SharedState::LOCKED, Time());
  subtle::Release_Store(&SharedStateFromSharedMemory(shared_memory_)->value.i,
                        new_state.value.i);
  return true;
}

bool DiscardableSharedMemory::Map(size_t size) {
  if (!shared_memory_.Map(AlignToPageSize(sizeof(SharedState)) + size))
    return false;

  mapped_size_ =
      shared_memory_.mapped_size() - AlignToPageSize(sizeof(SharedState));

  locked_page_count_ = AlignToPageSize(mapped_size_) / base::GetPageSize();
#if DCHECK_IS_ON()
  for (size_t page = 0; page < locked_page_count_; ++page)
    locked_pages_.insert(page);
#endif

  return true;
}

bool DiscardableSharedMemory::Unmap() {
  if (!shared_memory_.Unmap())
    return false;

  locked_page_count_ = 0;
#if DCHECK_IS_ON()
  locked_pages_.clear();
#endif
  mapped_size_ = 0;
  return true;
}

DiscardableSharedMemory::LockResult DiscardableSharedMemory::Lock(
    size_t offset, size_t length) {
  DCHECK_EQ(AlignToPageSize(offset), offset);
  DCHECK_EQ(AlignToPageSize(length), length);

  // Calls to this function must be synchronized properly.
  DFAKE_SCOPED_LOCK(thread_collision_warner_);

  DCHECK(shared_memory_.memory());

  // We need to successfully acquire the platform independent lock before
  // individual pages can be locked.
  if (!locked_page_count_) {
    // Return false when instance has been purged or not initialized properly
    // by checking if |last_known_usage_| is NULL.
    if (last_known_usage_.is_null())
      return FAILED;

    SharedState old_state(SharedState::UNLOCKED, last_known_usage_);
    SharedState new_state(SharedState::LOCKED, Time());
    SharedState result(subtle::Acquire_CompareAndSwap(
        &SharedStateFromSharedMemory(shared_memory_)->value.i,
        old_state.value.i,
        new_state.value.i));
    if (result.value.u != old_state.value.u) {
      // Update |last_known_usage_| in case the above CAS failed because of
      // an incorrect timestamp.
      last_known_usage_ = result.GetTimestamp();
      return FAILED;
    }
  }

  // Zero for length means "everything onward".
  if (!length)
    length = AlignToPageSize(mapped_size_) - offset;

  size_t start = offset / base::GetPageSize();
  size_t end = start + length / base::GetPageSize();
  DCHECK_LE(start, end);
  DCHECK_LE(end, AlignToPageSize(mapped_size_) / base::GetPageSize());

  // Add pages to |locked_page_count_|.
  // Note: Locking a page that is already locked is an error.
  locked_page_count_ += end - start;
#if DCHECK_IS_ON()
  // Detect incorrect usage by keeping track of exactly what pages are locked.
  for (auto page = start; page < end; ++page) {
    auto result = locked_pages_.insert(page);
    DCHECK(result.second);
  }
  DCHECK_EQ(locked_pages_.size(), locked_page_count_);
#endif

  // Always behave as if memory was purged when trying to lock a 0 byte segment.
  if (!length)
      return PURGED;

  // Ensure that the platform won't discard the required pages.
  return LockPages(shared_memory_,
                   AlignToPageSize(sizeof(SharedState)) + offset, length);
}

void DiscardableSharedMemory::Unlock(size_t offset, size_t length) {
  DCHECK_EQ(AlignToPageSize(offset), offset);
  DCHECK_EQ(AlignToPageSize(length), length);

  // Calls to this function must be synchronized properly.
  DFAKE_SCOPED_LOCK(thread_collision_warner_);

  // Passing zero for |length| means "everything onward". Note that |length| may
  // still be zero after this calculation, e.g. if |mapped_size_| is zero.
  if (!length)
    length = AlignToPageSize(mapped_size_) - offset;

  DCHECK(shared_memory_.memory());

  // Allow the pages to be discarded by the platform, if supported.
  UnlockPages(shared_memory_, AlignToPageSize(sizeof(SharedState)) + offset,
              length);

  size_t start = offset / base::GetPageSize();
  size_t end = start + length / base::GetPageSize();
  DCHECK_LE(start, end);
  DCHECK_LE(end, AlignToPageSize(mapped_size_) / base::GetPageSize());

  // Remove pages from |locked_page_count_|.
  // Note: Unlocking a page that is not locked is an error.
  DCHECK_GE(locked_page_count_, end - start);
  locked_page_count_ -= end - start;
#if DCHECK_IS_ON()
  // Detect incorrect usage by keeping track of exactly what pages are locked.
  for (auto page = start; page < end; ++page) {
    auto erased_count = locked_pages_.erase(page);
    DCHECK_EQ(1u, erased_count);
  }
  DCHECK_EQ(locked_pages_.size(), locked_page_count_);
#endif

  // Early out and avoid releasing the platform independent lock if some pages
  // are still locked.
  if (locked_page_count_)
    return;

  Time current_time = Now();
  DCHECK(!current_time.is_null());

  SharedState old_state(SharedState::LOCKED, Time());
  SharedState new_state(SharedState::UNLOCKED, current_time);
  // Note: timestamp cannot be NULL as that is a unique value used when
  // locked or purged.
  DCHECK(!new_state.GetTimestamp().is_null());
  // Timestamp precision should at least be accurate to the second.
  DCHECK_EQ((new_state.GetTimestamp() - Time::UnixEpoch()).InSeconds(),
            (current_time - Time::UnixEpoch()).InSeconds());
  SharedState result(subtle::Release_CompareAndSwap(
      &SharedStateFromSharedMemory(shared_memory_)->value.i,
      old_state.value.i,
      new_state.value.i));

  DCHECK_EQ(old_state.value.u, result.value.u);

  last_known_usage_ = current_time;
}

void* DiscardableSharedMemory::memory() const {
  return reinterpret_cast<uint8_t*>(shared_memory_.memory()) +
         AlignToPageSize(sizeof(SharedState));
}

bool DiscardableSharedMemory::Purge(Time current_time) {
  // Calls to this function must be synchronized properly.
  DFAKE_SCOPED_LOCK(thread_collision_warner_);
  DCHECK(shared_memory_.memory());

  SharedState old_state(SharedState::UNLOCKED, last_known_usage_);
  SharedState new_state(SharedState::UNLOCKED, Time());
  SharedState result(subtle::Acquire_CompareAndSwap(
      &SharedStateFromSharedMemory(shared_memory_)->value.i,
      old_state.value.i,
      new_state.value.i));

  // Update |last_known_usage_| to |current_time| if the memory is locked. This
  // allows the caller to determine if purging failed because last known usage
  // was incorrect or memory was locked. In the second case, the caller should
  // most likely wait for some amount of time before attempting to purge the
  // the memory again.
  if (result.value.u != old_state.value.u) {
    last_known_usage_ = result.GetLockState() == SharedState::LOCKED
                            ? current_time
                            : result.GetTimestamp();
    return false;
  }

// The next section will release as much resource as can be done
// from the purging process, until the client process notices the
// purge and releases its own references.
// Note: this memory will not be accessed again.  The segment will be
// freed asynchronously at a later time, so just do the best
// immediately.
#if defined(OS_POSIX) && !defined(OS_NACL)
// Linux and Android provide MADV_REMOVE which is preferred as it has a
// behavior that can be verified in tests. Other POSIX flavors (MacOSX, BSDs),
// provide MADV_FREE which has the same result but memory is purged lazily.
#if defined(OS_LINUX) || defined(OS_ANDROID)
#define MADV_PURGE_ARGUMENT MADV_REMOVE
#elif defined(OS_MACOSX)
// MADV_FREE_REUSABLE is similar to MADV_FREE, but also marks the pages with the
// reusable bit, which allows both Activity Monitor and memory-infra to
// correctly track the pages.
#define MADV_PURGE_ARGUMENT MADV_FREE_REUSABLE
#else
#define MADV_PURGE_ARGUMENT MADV_FREE
#endif
  // Advise the kernel to remove resources associated with purged pages.
  // Subsequent accesses of memory pages will succeed, but might result in
  // zero-fill-on-demand pages.
  if (madvise(reinterpret_cast<char*>(shared_memory_.memory()) +
                  AlignToPageSize(sizeof(SharedState)),
              AlignToPageSize(mapped_size_), MADV_PURGE_ARGUMENT)) {
    DPLOG(ERROR) << "madvise() failed";
  }
#elif defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8_1) {
    // Discard the purged pages, which releases the physical storage (resident
    // memory, compressed or swapped), but leaves them reserved & committed.
    // This does not free commit for use by other applications, but allows the
    // system to avoid compressing/swapping these pages to free physical memory.
    static const auto discard_virtual_memory =
        reinterpret_cast<decltype(&::DiscardVirtualMemory)>(GetProcAddress(
            GetModuleHandle(L"kernel32.dll"), "DiscardVirtualMemory"));
    if (discard_virtual_memory) {
      DWORD discard_result = discard_virtual_memory(
          reinterpret_cast<char*>(shared_memory_.memory()) +
              AlignToPageSize(sizeof(SharedState)),
          AlignToPageSize(mapped_size_));
      if (discard_result != ERROR_SUCCESS) {
        DLOG(DCHECK) << "DiscardVirtualMemory() failed in Purge(): "
                     << logging::SystemErrorCodeToString(discard_result);
      }
    }
  }
#endif

  last_known_usage_ = Time();
  return true;
}

bool DiscardableSharedMemory::IsMemoryResident() const {
  DCHECK(shared_memory_.memory());

  SharedState result(subtle::NoBarrier_Load(
      &SharedStateFromSharedMemory(shared_memory_)->value.i));

  return result.GetLockState() == SharedState::LOCKED ||
         !result.GetTimestamp().is_null();
}

bool DiscardableSharedMemory::IsMemoryLocked() const {
  DCHECK(shared_memory_.memory());

  SharedState result(subtle::NoBarrier_Load(
      &SharedStateFromSharedMemory(shared_memory_)->value.i));

  return result.GetLockState() == SharedState::LOCKED;
}

void DiscardableSharedMemory::Close() {
  shared_memory_.Close();
}

void DiscardableSharedMemory::CreateSharedMemoryOwnershipEdge(
    trace_event::MemoryAllocatorDump* local_segment_dump,
    trace_event::ProcessMemoryDump* pmd,
    bool is_owned) const {
  auto* shared_memory_dump =
      SharedMemoryTracker::GetOrCreateSharedMemoryDump(&shared_memory_, pmd);
  // TODO(ssid): Clean this by a new api to inherit size of parent dump once the
  // we send the full PMD and calculate sizes inside chrome, crbug.com/704203.
  size_t resident_size = shared_memory_dump->GetSizeInternal();
  local_segment_dump->AddScalar(trace_event::MemoryAllocatorDump::kNameSize,
                                trace_event::MemoryAllocatorDump::kUnitsBytes,
                                resident_size);

  // By creating an edge with a higher |importance| (w.r.t non-owned dumps)
  // the tracing UI will account the effective size of the segment to the
  // client instead of manager.
  // TODO(ssid): Define better constants in MemoryAllocatorDump for importance
  // values, crbug.com/754793.
  const int kImportance = is_owned ? 2 : 0;
  auto shared_memory_guid = shared_memory_.mapped_id();
  local_segment_dump->AddString("id", "hash", shared_memory_guid.ToString());

  // Owned discardable segments which are allocated by client process, could
  // have been cleared by the discardable manager. So, the segment need not
  // exist in memory and weak dumps are created to indicate the UI that the dump
  // should exist only if the manager also created the global dump edge.
  if (is_owned) {
    pmd->CreateWeakSharedMemoryOwnershipEdge(local_segment_dump->guid(),
                                             shared_memory_guid, kImportance);
  } else {
    pmd->CreateSharedMemoryOwnershipEdge(local_segment_dump->guid(),
                                         shared_memory_guid, kImportance);
  }
}

Time DiscardableSharedMemory::Now() const {
  return Time::Now();
}

}  // namespace base
