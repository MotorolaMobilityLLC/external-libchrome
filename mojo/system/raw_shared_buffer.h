// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_RAW_SHARED_BUFFER_H_
#define MOJO_SYSTEM_RAW_SHARED_BUFFER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/embedder/scoped_platform_handle.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

// |RawSharedBuffer| is a wrapper around OS-specific shared memory. It has the
// following features:
//   - A |RawSharedBuffer| simply represents a piece of shared memory that *may*
//     be mapped and *may* be shared to another process.
//   - A single |RawSharedBuffer| may be mapped multiple times. The lifetime of
//     the mapping (owned by |RawSharedBuffer::Mapping|) is separate from the
//     lifetime of the |RawSharedBuffer|.
//   - Sizes/offsets (of the shared memory and mappings) are arbitrary, and not
//     restricted by page size. However, more memory may actually be mapped than
//     requested.
//
// It currently does NOT support the following:
//   - Sharing read-only. (This will probably eventually be supported.)
//
// A |RawSharedBuffer| is not thread-safe (but is thread-friendly).
//
// TODO(vtl): Rectify this with |base::SharedMemory|.
class MOJO_SYSTEM_IMPL_EXPORT RawSharedBuffer {
 public:
  // A mapping of a |RawSharedBuffer| (compararable to a "file view" in
  // Windows); see above. Created by |RawSharedBuffer::Map()|. Automatically
  // unmaps memory on destruction.
  class MOJO_SYSTEM_IMPL_EXPORT Mapping {
   public:
    ~Mapping() { Unmap(); }

    void* base() const { return base_; }
    size_t length() const { return length_; }

   private:
    friend class RawSharedBuffer;

    Mapping(void* base, size_t length, void* real_base, size_t real_length)
        : base_(base), length_(length),
          real_base_(real_base), real_length_(real_length) {}
    void Unmap();

    void* const base_;
    const size_t length_;

    void* const real_base_;
    const size_t real_length_;

    DISALLOW_COPY_AND_ASSIGN(Mapping);
  };

  ~RawSharedBuffer();

  // Creates a shared buffer of size |num_bytes| bytes (initially zero-filled).
  // Returns null on failure.
  static scoped_ptr<RawSharedBuffer> Create(size_t num_bytes);

  // Maps (some) of the shared buffer into memory; [|offset|, |offset + length|]
  // must be contained in [0, |num_bytes|], and |length| must be at least 1.
  // Returns null on failure.
  scoped_ptr<Mapping> Map(size_t offset, size_t length);

  size_t num_bytes() const { return num_bytes_; }

 private:
  explicit RawSharedBuffer(size_t num_bytes);
  bool Init();
  // The platform-dependent part of |Map()|; doesn't check arguments.
  // TODO(vtl)
  scoped_ptr<Mapping> MapImpl(size_t offset, size_t length);

  size_t num_bytes_;
  embedder::ScopedPlatformHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(RawSharedBuffer);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_RAW_SHARED_BUFFER_H_
