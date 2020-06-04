// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/shared_buffer_dispatcher.h"

#include <limits>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/system/constants.h"
#include "mojo/system/memory.h"

namespace mojo {
namespace system {

// static
MojoResult SharedBufferDispatcher::ValidateOptions(
    const MojoCreateSharedBufferOptions* in_options,
    MojoCreateSharedBufferOptions* out_options) {
  static const MojoCreateSharedBufferOptions kDefaultOptions = {
    static_cast<uint32_t>(sizeof(MojoCreateSharedBufferOptions)),
    MOJO_CREATE_SHARED_BUFFER_OPTIONS_FLAG_NONE
  };
  if (!in_options) {
    *out_options = kDefaultOptions;
    return MOJO_RESULT_OK;
  }

  if (in_options->struct_size < sizeof(*in_options))
    return MOJO_RESULT_INVALID_ARGUMENT;
  out_options->struct_size = static_cast<uint32_t>(sizeof(*out_options));

  // All flags are okay (unrecognized flags will be ignored).
  out_options->flags = in_options->flags;

  return MOJO_RESULT_OK;
}

// static
MojoResult SharedBufferDispatcher::Create(
    const MojoCreateSharedBufferOptions& /*validated_options*/,
    uint64_t num_bytes,
    scoped_refptr<SharedBufferDispatcher>* result) {
  if (num_bytes > kMaxSharedMemoryNumBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  scoped_refptr<RawSharedBuffer> shared_buffer(
      RawSharedBuffer::Create(static_cast<size_t>(num_bytes)));
  if (!shared_buffer) {
    // TODO(vtl): This error code is a bit of a guess. Maybe we should propagate
    // error codes more carefully.
    return MOJO_RESULT_RESOURCE_EXHAUSTED;
  }

  *result = new SharedBufferDispatcher(shared_buffer);
  return MOJO_RESULT_OK;
}

Dispatcher::Type SharedBufferDispatcher::GetType() const {
  return kTypeSharedBuffer;
}

SharedBufferDispatcher::SharedBufferDispatcher(
    scoped_refptr<RawSharedBuffer> shared_buffer)
    : shared_buffer_(shared_buffer) {
  DCHECK(shared_buffer_);
}

SharedBufferDispatcher::~SharedBufferDispatcher() {
}

void SharedBufferDispatcher::CloseImplNoLock() {
  DCHECK(shared_buffer_);
  shared_buffer_ = NULL;
}

scoped_refptr<Dispatcher>
    SharedBufferDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  DCHECK(shared_buffer_);
  scoped_refptr<RawSharedBuffer> shared_buffer;
  shared_buffer.swap(shared_buffer_);
  return scoped_refptr<Dispatcher>(new SharedBufferDispatcher(shared_buffer));
}

MojoResult SharedBufferDispatcher::DuplicateBufferHandleImplNoLock(
    const MojoDuplicateBufferHandleOptions* options,
    scoped_refptr<Dispatcher>* new_dispatcher) {
  if (options) {
    // The |struct_size| field must be valid to read.
    if (!VerifyUserPointer<uint32_t>(&options->struct_size, 1))
      return MOJO_RESULT_INVALID_ARGUMENT;
    // And then |options| must point to at least |options->struct_size| bytes.
    if (!VerifyUserPointer<void>(options, options->struct_size))
      return MOJO_RESULT_INVALID_ARGUMENT;

    if (options->struct_size < sizeof(*options))
      return MOJO_RESULT_INVALID_ARGUMENT;
    // We don't actually do anything with |options|.
  }

  *new_dispatcher = new SharedBufferDispatcher(shared_buffer_);
  return MOJO_RESULT_OK;
}

MojoResult SharedBufferDispatcher::MapBufferImplNoLock(
    uint64_t offset,
    uint64_t num_bytes,
    MojoMapBufferFlags flags,
    scoped_ptr<RawSharedBuffer::Mapping>* mapping) {
  DCHECK(shared_buffer_);

  if (offset > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (num_bytes > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    return MOJO_RESULT_INVALID_ARGUMENT;

  DCHECK(mapping);
  *mapping = shared_buffer_->Map(static_cast<size_t>(offset),
                                 static_cast<size_t>(num_bytes));
  if (!*mapping) {
    // TODO(vtl): This is just a guess; propagate errors more thoroughly.
    return MOJO_RESULT_INVALID_ARGUMENT;
  }

  return MOJO_RESULT_OK;
}

MojoWaitFlags SharedBufferDispatcher::SatisfiedFlagsNoLock() const {
  // TODO(vtl): Add transferrable flag.
  return MOJO_WAIT_FLAG_NONE;
}

MojoWaitFlags SharedBufferDispatcher::SatisfiableFlagsNoLock() const {
  // TODO(vtl): Add transferrable flag.
  return MOJO_WAIT_FLAG_NONE;
}

}  // namespace system
}  // namespace mojo
