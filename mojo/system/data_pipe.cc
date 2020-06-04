// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/data_pipe.h"

#include <string.h>

#include <algorithm>

#include "base/logging.h"
#include "mojo/system/memory.h"
#include "mojo/system/waiter_list.h"

namespace mojo {
namespace system {

void DataPipe::ProducerCancelAllWaiters() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_->CancelAllWaiters();
}

void DataPipe::ProducerClose() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_.reset();
  ProducerCloseImplNoLock();
}

MojoResult DataPipe::ProducerWriteData(const void* elements,
                                       uint32_t* num_elements,
                                       MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  void* buffer = NULL;
  uint32_t buffer_num_elements = *num_elements;
  MojoResult rv = ProducerBeginWriteDataImplNoLock(&buffer,
                                                   &buffer_num_elements,
                                                   flags);
  if (rv != MOJO_RESULT_OK)
    return rv;

  uint32_t num_elements_to_write = std::min(*num_elements, buffer_num_elements);
  memcpy(buffer, elements, num_elements_to_write * element_size_);

  rv = ProducerEndWriteDataImplNoLock(num_elements_to_write);
  if (rv != MOJO_RESULT_OK)
    return rv;

  *num_elements = num_elements_to_write;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ProducerBeginWriteData(void** buffer,
                                            uint32_t* buffer_num_elements,
                                            MojoWriteDataFlags flags) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  return ProducerBeginWriteDataImplNoLock(buffer, buffer_num_elements, flags);
}

MojoResult DataPipe::ProducerEndWriteData(uint32_t num_elements_written) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  return ProducerEndWriteDataImplNoLock(num_elements_written);
}

MojoResult DataPipe::ProducerAddWaiter(Waiter* waiter,
                                       MojoWaitFlags flags,
                                       MojoResult wake_result) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if ((flags & ProducerSatisfiedFlagsNoLock()))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!(flags & ProducerSatisfiableFlagsNoLock()))
    return MOJO_RESULT_FAILED_PRECONDITION;

  producer_waiter_list_->AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void DataPipe::ProducerRemoveWaiter(Waiter* waiter) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_->RemoveWaiter(waiter);
}

DataPipe::DataPipe(bool has_local_producer, bool has_local_consumer)
    : producer_waiter_list_(has_local_producer ? new WaiterList() : NULL),
      consumer_waiter_list_(has_local_consumer ? new WaiterList() : NULL) {
  DCHECK(has_local_producer || has_local_consumer);
}

DataPipe::~DataPipe() {
  DCHECK(!has_local_producer_no_lock());
  DCHECK(!has_local_consumer_no_lock());
}

}  // namespace system
}  // namespace mojo
