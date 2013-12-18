// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SYSTEM_DATA_PIPE_H_
#define MOJO_SYSTEM_DATA_PIPE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/public/system/core.h"
#include "mojo/system/system_impl_export.h"

namespace mojo {
namespace system {

class Waiter;
class WaiterList;

// |DataPipe| is a base class for secondary objects implementing data pipes,
// similar to |MessagePipe| (see the explanatory comment in core_impl.cc). It is
// typically owned by the dispatcher(s) corresponding to the local endpoints.
// Its subclasses implement the three cases: local producer and consumer, local
// producer and remote consumer, and remote producer and local consumer. This
// class is thread-safe.
class MOJO_SYSTEM_IMPL_EXPORT DataPipe :
    public base::RefCountedThreadSafe<DataPipe> {
 public:
  // Validates and/or sets default options. If non-null, |in_options| must point
  // to a struct of at least |in_options->struct_size| bytes. |out_options| must
  // point to a (current) |MojoCreateDataPipeOptions| and will be entirely
  // overwritten on success (it may be partly overwritten on failure).
  static MojoResult ValidateOptions(const MojoCreateDataPipeOptions* in_options,
                                    MojoCreateDataPipeOptions* out_options);

  // These are called by the producer dispatcher to implement its methods of
  // corresponding names.
  void ProducerCancelAllWaiters();
  void ProducerClose();
  // This does not validate its arguments, except to check that |*num_bytes| is
  // a multiple of |element_num_bytes_|.
  MojoResult ProducerWriteData(const void* elements,
                               uint32_t* num_bytes,
                               bool all_or_none);
  // This does not validate its arguments.
  MojoResult ProducerBeginWriteData(void** buffer,
                                    uint32_t* buffer_num_bytes,
                                    bool all_or_none);
  MojoResult ProducerEndWriteData(uint32_t num_bytes_written);
  MojoResult ProducerAddWaiter(Waiter* waiter,
                               MojoWaitFlags flags,
                               MojoResult wake_result);
  void ProducerRemoveWaiter(Waiter* waiter);

  // These are called by the consumer dispatcher to implement its methods of
  // corresponding names.
  void ConsumerCancelAllWaiters();
  void ConsumerClose();
  // This does not validate its arguments, except to check that |*num_bytes| is
  // a multiple of |element_num_bytes_|.
  MojoResult ConsumerReadData(void* elements,
                              uint32_t* num_bytes,
                              bool all_or_none);
  MojoResult ConsumerDiscardData(uint32_t* num_bytes,
                                 bool all_or_none);
  MojoResult ConsumerQueryData(uint32_t* num_bytes);
  // This does not validate its arguments.
  MojoResult ConsumerBeginReadData(const void** buffer,
                                   uint32_t* buffer_num_bytes,
                                   bool all_or_none);
  MojoResult ConsumerEndReadData(uint32_t num_bytes_read);
  MojoResult ConsumerAddWaiter(Waiter* waiter,
                               MojoWaitFlags flags,
                               MojoResult wake_result);
  void ConsumerRemoveWaiter(Waiter* waiter);

 protected:
  DataPipe(bool has_local_producer,
           bool has_local_consumer,
           const MojoCreateDataPipeOptions& validated_options);

  friend class base::RefCountedThreadSafe<DataPipe>;
  virtual ~DataPipe();

  void AwakeProducerWaitersForStateChangeNoLock();
  void AwakeConsumerWaitersForStateChangeNoLock();

  virtual void ProducerCloseImplNoLock() = 0;
  // |*num_bytes| will be a nonzero multiple of |element_num_bytes_|.
  virtual MojoResult ProducerWriteDataImplNoLock(const void* elements,
                                                 uint32_t* num_bytes,
                                                 bool all_or_none) = 0;
  virtual MojoResult ProducerBeginWriteDataImplNoLock(
      void** buffer,
      uint32_t* buffer_num_bytes,
      bool all_or_none) = 0;
  virtual MojoResult ProducerEndWriteDataImplNoLock(
      uint32_t num_bytes_written) = 0;
  virtual MojoWaitFlags ProducerSatisfiedFlagsNoLock() = 0;
  virtual MojoWaitFlags ProducerSatisfiableFlagsNoLock() = 0;

  virtual void ConsumerCloseImplNoLock() = 0;
  // |*num_bytes| will be a nonzero multiple of |element_num_bytes_|.
  virtual MojoResult ConsumerReadDataImplNoLock(void* elements,
                                                uint32_t* num_bytes,
                                                bool all_or_none) = 0;
  virtual MojoResult ConsumerDiscardDataImplNoLock(uint32_t* num_bytes,
                                                   bool all_or_none) = 0;
  // |*num_bytes| will be a nonzero multiple of |element_num_bytes_|.
  virtual MojoResult ConsumerQueryDataImplNoLock(uint32_t* num_bytes) = 0;
  virtual MojoResult ConsumerBeginReadDataImplNoLock(const void** buffer,
                                                     uint32_t* buffer_num_bytes,
                                                     bool all_or_none) = 0;
  virtual MojoResult ConsumerEndReadDataImplNoLock(uint32_t num_bytes_read) = 0;
  virtual MojoWaitFlags ConsumerSatisfiedFlagsNoLock() = 0;
  virtual MojoWaitFlags ConsumerSatisfiableFlagsNoLock() = 0;

  // Thread-safe and fast (they don't take the lock):
  // TODO(vtl): FIXME -- "may discard" not respected
  bool may_discard() const { return may_discard_; }
  size_t element_num_bytes() const { return element_num_bytes_; }
  size_t capacity_num_bytes() const { return capacity_num_bytes_; }

  // Must be called under lock.
  bool producer_open_no_lock() const {
    lock_.AssertAcquired();
    return producer_open_;
  }
  bool consumer_open_no_lock() const {
    lock_.AssertAcquired();
    return consumer_open_;
  }

 private:
  bool has_local_producer_no_lock() const {
    return !!producer_waiter_list_.get();
  }
  bool has_local_consumer_no_lock() const {
    return !!consumer_waiter_list_.get();
  }

  const bool may_discard_;
  const size_t element_num_bytes_;
  const size_t capacity_num_bytes_;

  base::Lock lock_;  // Protects the following members.
  // *Known* state of producer or consumer.
  bool producer_open_;
  bool consumer_open_;
  // Non-null only if the producer or consumer, respectively, is local.
  scoped_ptr<WaiterList> producer_waiter_list_;
  scoped_ptr<WaiterList> consumer_waiter_list_;
  bool producer_in_two_phase_write_;
  bool consumer_in_two_phase_read_;

  DISALLOW_COPY_AND_ASSIGN(DataPipe);
};

}  // namespace system
}  // namespace mojo

#endif  // MOJO_SYSTEM_DATA_PIPE_H_
