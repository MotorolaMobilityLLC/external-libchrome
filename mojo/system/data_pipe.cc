// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/data_pipe.h"

#include <string.h>

#include <algorithm>
#include <limits>

#include "base/logging.h"
#include "mojo/system/constants.h"
#include "mojo/system/memory.h"
#include "mojo/system/waiter_list.h"

namespace mojo {
namespace system {

// static
MojoResult DataPipe::ValidateOptions(
    const MojoCreateDataPipeOptions* in_options,
    MojoCreateDataPipeOptions* out_options) {
  static const MojoCreateDataPipeOptions kDefaultOptions = {
    static_cast<uint32_t>(sizeof(MojoCreateDataPipeOptions)),
    MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
    1u,
    static_cast<uint32_t>(kDefaultDataPipeCapacityBytes)
  };
  if (!in_options) {
    *out_options = kDefaultOptions;
    return MOJO_RESULT_OK;
  }

  if (in_options->struct_size < sizeof(*in_options))
    return MOJO_RESULT_INVALID_ARGUMENT;
  out_options->struct_size = static_cast<uint32_t>(sizeof(*out_options));

  if (in_options->element_num_bytes == 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  out_options->element_num_bytes = in_options->element_num_bytes;

  if (in_options->capacity_num_bytes == 0) {
    // Round the default capacity down to a multiple of the element size (but at
    // least one element).
    out_options->capacity_num_bytes = std::max(
        static_cast<uint32_t>(kDefaultDataPipeCapacityBytes -
            (kDefaultDataPipeCapacityBytes % in_options->element_num_bytes)),
        in_options->element_num_bytes);
  } else {
    if (in_options->capacity_num_bytes % in_options->element_num_bytes != 0)
      return MOJO_RESULT_INVALID_ARGUMENT;
    out_options->capacity_num_bytes = in_options->capacity_num_bytes;
  }
  if (out_options->capacity_num_bytes > kMaxDataPipeCapacityBytes)
    return MOJO_RESULT_RESOURCE_EXHAUSTED;

  return MOJO_RESULT_OK;
}

void DataPipe::ProducerCancelAllWaiters() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_->CancelAllWaiters();
}

void DataPipe::ProducerClose() {
  base::AutoLock locker(lock_);
  DCHECK(producer_open_);
  producer_open_ = false;
  DCHECK(has_local_producer_no_lock());
  producer_waiter_list_.reset();
  // TODO(vtl): FIXME -- "cancel" any two-phase write (do we need to do this?)
  ProducerCloseImplNoLock();
}

MojoResult DataPipe::ProducerWriteData(const void* elements,
                                       uint32_t* num_bytes,
                                       bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_)
    return MOJO_RESULT_BUSY;

  // Returning "busy" takes priority over "invalid argument".
  if (*num_bytes % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  return ProducerWriteDataImplNoLock(elements, num_bytes, all_or_none);
}

MojoResult DataPipe::ProducerBeginWriteData(void** buffer,
                                            uint32_t* buffer_num_bytes,
                                            bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (producer_in_two_phase_write_)
    return MOJO_RESULT_BUSY;

  MojoResult rv = ProducerBeginWriteDataImplNoLock(buffer, buffer_num_bytes,
                                                   all_or_none);
  if (rv != MOJO_RESULT_OK)
    return rv;

  producer_in_two_phase_write_ = true;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ProducerEndWriteData(uint32_t num_bytes_written) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_producer_no_lock());

  if (!producer_in_two_phase_write_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  MojoResult rv = ProducerEndWriteDataImplNoLock(num_bytes_written);
  producer_in_two_phase_write_ = false;  // End two-phase write even on failure.
  return rv;
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

void DataPipe::ConsumerCancelAllWaiters() {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_->CancelAllWaiters();
}

void DataPipe::ConsumerClose() {
  base::AutoLock locker(lock_);
  DCHECK(consumer_open_);
  consumer_open_ = false;
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_.reset();
  // TODO(vtl): FIXME -- "cancel" any two-phase read (do we need to do this?)
  ConsumerCloseImplNoLock();
}

MojoResult DataPipe::ConsumerReadData(void* elements,
                                      uint32_t* num_bytes,
                                      bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  if (*num_bytes % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  return ConsumerReadDataImplNoLock(elements, num_bytes, all_or_none);
}

MojoResult DataPipe::ConsumerDiscardData(uint32_t* num_bytes,
                                         bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  if (*num_bytes % element_num_bytes_ != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;

  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  return ConsumerDiscardDataImplNoLock(num_bytes, all_or_none);
}

MojoResult DataPipe::ConsumerQueryData(uint32_t* num_bytes) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  // Note: Don't need to validate |*num_bytes| for query.
  return ConsumerQueryDataImplNoLock(num_bytes);
}

MojoResult DataPipe::ConsumerBeginReadData(const void** buffer,
                                           uint32_t* buffer_num_bytes,
                                           bool all_or_none) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (consumer_in_two_phase_read_)
    return MOJO_RESULT_BUSY;

  MojoResult rv = ConsumerBeginReadDataImplNoLock(buffer, buffer_num_bytes,
                                                  all_or_none);
  if (rv != MOJO_RESULT_OK)
    return rv;

  consumer_in_two_phase_read_ = true;
  return MOJO_RESULT_OK;
}

MojoResult DataPipe::ConsumerEndReadData(uint32_t num_bytes_read) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if (!consumer_in_two_phase_read_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  MojoResult rv = ConsumerEndReadDataImplNoLock(num_bytes_read);
  consumer_in_two_phase_read_ = false;  // End two-phase read even on failure.
  return rv;
}

MojoResult DataPipe::ConsumerAddWaiter(Waiter* waiter,
                                       MojoWaitFlags flags,
                                       MojoResult wake_result) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());

  if ((flags & ConsumerSatisfiedFlagsNoLock()))
    return MOJO_RESULT_ALREADY_EXISTS;
  if (!(flags & ConsumerSatisfiableFlagsNoLock()))
    return MOJO_RESULT_FAILED_PRECONDITION;

  consumer_waiter_list_->AddWaiter(waiter, flags, wake_result);
  return MOJO_RESULT_OK;
}

void DataPipe::ConsumerRemoveWaiter(Waiter* waiter) {
  base::AutoLock locker(lock_);
  DCHECK(has_local_consumer_no_lock());
  consumer_waiter_list_->RemoveWaiter(waiter);
}

DataPipe::DataPipe(bool has_local_producer,
                   bool has_local_consumer,
                   const MojoCreateDataPipeOptions& validated_options)
    : may_discard_((validated_options.flags &
                       MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_MAY_DISCARD)),
      element_num_bytes_(validated_options.element_num_bytes),
      capacity_num_bytes_(validated_options.capacity_num_bytes),
      producer_open_(true),
      consumer_open_(true),
      producer_waiter_list_(has_local_producer ? new WaiterList() : NULL),
      consumer_waiter_list_(has_local_consumer ? new WaiterList() : NULL),
      producer_in_two_phase_write_(false),
      consumer_in_two_phase_read_(false) {
//FIXME
}

DataPipe::~DataPipe() {
  DCHECK(!producer_open_);
  DCHECK(!consumer_open_);
  DCHECK(!has_local_producer_no_lock());
  DCHECK(!has_local_consumer_no_lock());
}

void DataPipe::AwakeProducerWaitersForStateChangeNoLock() {
  lock_.AssertAcquired();
  if (!has_local_producer_no_lock())
    return;
  producer_waiter_list_->AwakeWaitersForStateChange(
      ProducerSatisfiedFlagsNoLock(), ProducerSatisfiableFlagsNoLock());
}

void DataPipe::AwakeConsumerWaitersForStateChangeNoLock() {
  lock_.AssertAcquired();
  if (!has_local_consumer_no_lock())
    return;
  consumer_waiter_list_->AwakeWaitersForStateChange(
      ConsumerSatisfiedFlagsNoLock(), ConsumerSatisfiableFlagsNoLock());
}

}  // namespace system
}  // namespace mojo
