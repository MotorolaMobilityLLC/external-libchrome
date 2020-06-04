// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_SCRATCH_BUFFER_H_
#define MOJO_PUBLIC_BINDINGS_LIB_SCRATCH_BUFFER_H_

#include <deque>

#include "mojo/public/bindings/buffer.h"
#include "mojo/public/c/system/macros.h"

namespace mojo {
namespace internal {

// The following class is designed to be allocated on the stack.  If necessary,
// it will failover to allocating objects on the heap.
class ScratchBuffer : public Buffer {
 public:
  ScratchBuffer();
  virtual ~ScratchBuffer();

  virtual void* Allocate(size_t num_bytes, Destructor func = NULL)
      MOJO_OVERRIDE;

 private:
  enum { kMinSegmentSize = 512 };

  struct Segment {
    Segment* next;
    char* cursor;
    char* end;
  };

  void* AllocateInSegment(Segment* segment, size_t num_bytes);
  void AddOverflowSegment(size_t delta);

  char fixed_data_[kMinSegmentSize];
  Segment fixed_;
  Segment* overflow_;

  struct PendingDestructor {
    Destructor func;
    void* address;
  };
  std::deque<PendingDestructor> pending_dtors_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(ScratchBuffer);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_SCRATCH_BUFFER_H_
