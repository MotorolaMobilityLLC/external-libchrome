// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/data_pipe_consumer_dispatcher.h"

#include "base/logging.h"
#include "mojo/system/data_pipe.h"
#include "mojo/system/memory.h"

namespace mojo {
namespace system {

DataPipeConsumerDispatcher::DataPipeConsumerDispatcher() {
}

void DataPipeConsumerDispatcher::Init(scoped_refptr<DataPipe> data_pipe) {
  DCHECK(data_pipe.get());
  data_pipe_ = data_pipe;
}

DataPipeConsumerDispatcher::~DataPipeConsumerDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the pipe.
  DCHECK(!data_pipe_.get());
}

void DataPipeConsumerDispatcher::CancelAllWaitersNoLock() {
  lock().AssertAcquired();
  data_pipe_->ConsumerCancelAllWaiters();
}

MojoResult DataPipeConsumerDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  data_pipe_->ConsumerClose();
  data_pipe_ = NULL;
  return MOJO_RESULT_OK;
}

MojoResult DataPipeConsumerDispatcher::ReadDataImplNoLock(
    void* elements,
    uint32_t* num_elements,
    MojoReadDataFlags flags) {
  lock().AssertAcquired();

  if (!VerifyUserPointer<uint32_t>(num_elements, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;
  // These flags are mutally exclusive.
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD) &&
      (flags & MOJO_READ_DATA_FLAG_QUERY))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD) ||
      (flags & MOJO_READ_DATA_FLAG_QUERY)) {
    DVLOG_IF(2, elements) << "Discard/query mode: ignoring non-null |elements|";
    elements = NULL;  // Null it out for safety.
  } else {
    // Only verify |elements| if we're neither discarding nor querying.
    if (!VerifyUserPointerForSize(elements, data_pipe_->element_size(),
                                  *num_elements))
      return MOJO_RESULT_INVALID_ARGUMENT;
  }

  return data_pipe_->ConsumerReadData(elements, num_elements, flags);
}

MojoResult DataPipeConsumerDispatcher::BeginReadDataImplNoLock(
    const void** buffer,
    uint32_t* buffer_num_elements,
    MojoReadDataFlags flags) {
  lock().AssertAcquired();

  if (!VerifyUserPointer<const void*>(buffer, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (!VerifyUserPointer<uint32_t>(buffer_num_elements, 1))
    return MOJO_RESULT_INVALID_ARGUMENT;
  // These flags may not be used in two-phase mode.
  if ((flags & MOJO_READ_DATA_FLAG_DISCARD) ||
      (flags & MOJO_READ_DATA_FLAG_QUERY))
    return MOJO_RESULT_INVALID_ARGUMENT;

  return data_pipe_->ConsumerBeginReadData(buffer, buffer_num_elements, flags);
}

MojoResult DataPipeConsumerDispatcher::EndReadDataImplNoLock(
    uint32_t num_elements_read) {
  lock().AssertAcquired();

  return data_pipe_->ConsumerEndReadData(num_elements_read);
}

MojoResult DataPipeConsumerDispatcher::AddWaiterImplNoLock(
    Waiter* waiter,
    MojoWaitFlags flags,
    MojoResult wake_result) {
  lock().AssertAcquired();
  return data_pipe_->ConsumerAddWaiter(waiter, flags, wake_result);
}

void DataPipeConsumerDispatcher::RemoveWaiterImplNoLock(Waiter* waiter) {
  lock().AssertAcquired();
  data_pipe_->ConsumerRemoveWaiter(waiter);
}

scoped_refptr<Dispatcher>
DataPipeConsumerDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  scoped_refptr<DataPipeConsumerDispatcher> rv =
      new DataPipeConsumerDispatcher();
  rv->Init(data_pipe_);
  data_pipe_ = NULL;
  return scoped_refptr<Dispatcher>(rv.get());
}

}  // namespace system
}  // namespace mojo
