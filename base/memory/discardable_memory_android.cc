// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include <sys/mman.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "third_party/ashmem/ashmem.h"

namespace base {

DiscardableMemory::DiscardableMemory()
    : fd_(-1),
      memory_(NULL),
      size_(0),
      is_pinned_(false) {
  DCHECK(Supported());
}

DiscardableMemory::~DiscardableMemory() {
  if (is_pinned_)
    Unlock();
  if (fd_ < 0)
    return;
  HANDLE_EINTR(close(fd_));
  fd_ = -1;
}

bool DiscardableMemory::InitializeAndLock(size_t size) {
  DCHECK_EQ(fd_, -1);
  DCHECK(!memory_);
  size_ = size;
  fd_ = ashmem_create_region("", size);

  if (fd_ < 0) {
    DLOG(ERROR) << "ashmem_create_region() failed";
    return false;
  }

  int err = ashmem_set_prot_region(fd_, PROT_READ | PROT_WRITE);
  if (err < 0) {
    DLOG(ERROR) << "Error " << err << " when setting protection of ashmem";
    HANDLE_EINTR(close(fd_));
    fd_ = -1;
    return false;
  }

  if (!Map()) {
    return false;
  }

  is_pinned_ = true;
  return true;
}

LockDiscardableMemoryStatus DiscardableMemory::Lock() {
  DCHECK_NE(fd_, -1);
  DCHECK(!is_pinned_);

  bool purged = false;
  if (ashmem_pin_region(fd_, 0, 0) == ASHMEM_WAS_PURGED)
    purged = true;

  if (!Map())
    return FAILED;

  is_pinned_ = true;
  return purged ? PURGED : SUCCESS;
}

void DiscardableMemory::Unlock() {
  DCHECK_GE(fd_, 0);
  DCHECK(is_pinned_);

  Unmap();
  if (ashmem_unpin_region(fd_, 0, 0))
    DLOG(ERROR) << "Failed to unpin memory.";
  is_pinned_ = false;
}

void* DiscardableMemory::Memory() const {
  DCHECK(is_pinned_);
  return memory_;
}

bool DiscardableMemory::Map() {
  DCHECK(!memory_);
  // There is a problem using MAP_PRIVATE here. As we are constantly calling
  // Lock() and Unlock(), data could get lost if they are not written to the
  // underlying file when Unlock() gets called.
  memory_ = mmap(NULL, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
  if (memory_ == (void*)-1) {
    DPLOG(ERROR) << "Failed to map memory.";
    memory_ = NULL;
    if (ashmem_unpin_region(fd_, 0, 0))
      DLOG(ERROR) << "Failed to unpin memory.";
    return false;
  }
  return true;
}

void DiscardableMemory::Unmap() {
  DCHECK(memory_);

  if (-1 == munmap(memory_, size_))
    DPLOG(ERROR) << "Failed to unmap memory.";

  memory_ = NULL;
}

}  // namespace skia
