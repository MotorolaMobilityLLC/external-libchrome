// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_CORE_H_
#define MOJO_PUBLIC_C_SYSTEM_CORE_H_

#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/functions.h"
#include "mojo/public/c/system/macros.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Types/constants -------------------------------------------------------------

// TODO(vtl): Split these into their own header files.

// Shared buffer:

// |MojoCreateSharedBufferOptions|: Used to specify creation parameters for a
// shared buffer to |MojoCreateSharedBuffer()|.
//   |uint32_t struct_size|: Set to the size of the
//       |MojoCreateSharedBufferOptions| struct. (Used to allow for future
//       extensions.)
//   |MojoCreateSharedBufferOptionsFlags flags|: Reserved for future use.
//       |MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE|: No flags; default mode.
//
// TODO(vtl): Maybe add a flag to indicate whether the memory should be
// executable or not?
// TODO(vtl): Also a flag for discardable (ashmem-style) buffers.

typedef uint32_t MojoCreateSharedBufferOptionsFlags;

#ifdef __cplusplus
const MojoCreateSharedBufferOptionsFlags
    MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE = 0;
#else
#define MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE \
    ((MojoCreateSharedBufferOptionsFlags) 0)
#endif

struct MojoCreateSharedBufferOptions {
  uint32_t struct_size;
  MojoCreateSharedBufferOptionsFlags flags;
};
MOJO_COMPILE_ASSERT(sizeof(MojoCreateSharedBufferOptions) == 8,
                    MojoCreateSharedBufferOptions_has_wrong_size);

// |MojoDuplicateBufferHandleOptions|: Used to specify parameters in duplicating
// access to a shared buffer to |MojoDuplicateBufferHandle()|.
//   |uint32_t struct_size|: Set to the size of the
//       |MojoDuplicateBufferHandleOptions| struct. (Used to allow for future
//       extensions.)
//   |MojoDuplicateBufferHandleOptionsFlags flags|: Reserved for future use.
//       |MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE|: No flags; default
//       mode.
//
// TODO(vtl): Add flags to remove writability (and executability)? Also, COW?

typedef uint32_t MojoDuplicateBufferHandleOptionsFlags;

#ifdef __cplusplus
const MojoDuplicateBufferHandleOptionsFlags
    MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE = 0;
#else
#define MOJO_DUPLICATE_BUFFER_HANDLE_OPTIONS_FLAG_NONE \
    ((MojoDuplicateBufferHandleOptionsFlags) 0)
#endif

struct MojoDuplicateBufferHandleOptions {
  uint32_t struct_size;
  MojoDuplicateBufferHandleOptionsFlags flags;
};
MOJO_COMPILE_ASSERT(sizeof(MojoDuplicateBufferHandleOptions) == 8,
                    MojoDuplicateBufferHandleOptions_has_wrong_size);

// |MojoMapBufferFlags|: Used to specify different modes to |MojoMapBuffer()|.
//   |MOJO_MAP_BUFFER_FLAG_NONE| - No flags; default mode.

typedef uint32_t MojoMapBufferFlags;

#ifdef __cplusplus
const MojoMapBufferFlags MOJO_MAP_BUFFER_FLAG_NONE = 0;
#else
#define MOJO_MAP_BUFFER_FLAG_NONE ((MojoMapBufferFlags) 0)
#endif

// Functions -------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

// Note: See the comment in functions.h about the meaning of the "optional"
// label for pointer parameters.

// Shared buffer:

// TODO(vtl): General comments.

// Creates a buffer that can be shared between applications (by duplicating the
// handle -- see |MojoDuplicateBufferHandle()| -- and passing it over a message
// pipe). To access the buffer, one must call |MojoMapBuffer()|.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoCreateSharedBuffer(
    const struct MojoCreateSharedBufferOptions* options,  // Optional.
    uint64_t num_bytes,  // In.
    MojoHandle* shared_buffer_handle);  // Out.

// Duplicates the handle |buffer_handle| to a buffer. This creates another
// handle (returned in |*new_buffer_handle| on success), which can then be sent
// to another application over a message pipe, while retaining access to the
// |buffer_handle| (and any mappings that it may have).
//
// Note: We may add buffer types for which this operation is not supported.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoDuplicateBufferHandle(
    MojoHandle buffer_handle,
    const struct MojoDuplicateBufferHandleOptions* options,  // Optional.
    MojoHandle* new_buffer_handle);  // Out.

// Map the part (at offset |offset| of length |num_bytes|) of the buffer given
// by |buffer_handle| into memory. |offset + num_bytes| must be less than or
// equal to the size of the buffer. On success, |*buffer| points to memory with
// the requested part of the buffer.
//
// A single buffer handle may have multiple active mappings (possibly depending
// on the buffer type). The permissions (e.g., writable or executable) of the
// returned memory may depend on the properties of the buffer and properties
// attached to the buffer handle as well as |flags|.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoMapBuffer(MojoHandle buffer_handle,
                                            uint64_t offset,
                                            uint64_t num_bytes,
                                            void** buffer,  // Out.
                                            MojoMapBufferFlags flags);

// Unmap a buffer pointer that was mapped by |MojoMapBuffer()|.
// TODO(vtl): More.
MOJO_SYSTEM_EXPORT MojoResult MojoUnmapBuffer(void* buffer);  // In.

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_CORE_H_
