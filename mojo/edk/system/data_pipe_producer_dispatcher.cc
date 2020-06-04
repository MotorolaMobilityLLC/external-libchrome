// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/data_pipe_producer_dispatcher.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/data_pipe.h"

namespace mojo {
namespace edk {

void DataPipeProducerDispatcher::Init(ScopedPlatformHandle message_pipe) {
  if (message_pipe.is_valid()) {
    channel_ = RawChannel::Create(message_pipe.Pass());
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE, base::Bind(&DataPipeProducerDispatcher::InitOnIO, this));
  }
}

void DataPipeProducerDispatcher::InitOnIO() {
  base::AutoLock locker(lock());
  if (channel_)
    channel_->Init(this);
}

void DataPipeProducerDispatcher::CloseOnIO() {
  base::AutoLock locker(lock());
  if (channel_) {
    channel_->Shutdown();
    channel_ = nullptr;
  }
}

Dispatcher::Type DataPipeProducerDispatcher::GetType() const {
  return Type::DATA_PIPE_PRODUCER;
}

scoped_refptr<DataPipeProducerDispatcher>
DataPipeProducerDispatcher::Deserialize(
    const void* source,
    size_t size,
    PlatformHandleVector* platform_handles) {
  MojoCreateDataPipeOptions options;
  ScopedPlatformHandle platform_handle =
      DataPipe::Deserialize(source, size, platform_handles, &options,
        nullptr, 0);

  scoped_refptr<DataPipeProducerDispatcher> rv(Create(options));
  if (platform_handle.is_valid())
    rv->Init(platform_handle.Pass());
  return rv;
}

DataPipeProducerDispatcher::DataPipeProducerDispatcher(
    const MojoCreateDataPipeOptions& options)
    : options_(options), channel_(nullptr), error_(false) {
}

DataPipeProducerDispatcher::~DataPipeProducerDispatcher() {
  // |Close()|/|CloseImplNoLock()| should have taken care of the channel.
  DCHECK(!channel_);
}

void DataPipeProducerDispatcher::CancelAllAwakablesNoLock() {
  lock().AssertAcquired();
  awakable_list_.CancelAll();
}

void DataPipeProducerDispatcher::CloseImplNoLock() {
  lock().AssertAcquired();
  internal::g_io_thread_task_runner->PostTask(
      FROM_HERE, base::Bind(&DataPipeProducerDispatcher::CloseOnIO, this));
}

scoped_refptr<Dispatcher>
DataPipeProducerDispatcher::CreateEquivalentDispatcherAndCloseImplNoLock() {
  lock().AssertAcquired();

  scoped_refptr<DataPipeProducerDispatcher> rv = Create(options_);
  rv->channel_ = channel_;
  channel_ = nullptr;
  rv->options_ = options_;
  return scoped_refptr<Dispatcher>(rv.get());
}

MojoResult DataPipeProducerDispatcher::WriteDataImplNoLock(
    const void* elements,
    uint32_t* num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();
  if (InTwoPhaseWrite())
    return MOJO_RESULT_BUSY;
  if (error_)
    return MOJO_RESULT_FAILED_PRECONDITION;
  if (*num_bytes % options_.element_num_bytes != 0)
    return MOJO_RESULT_INVALID_ARGUMENT;
  if (*num_bytes == 0)
    return MOJO_RESULT_OK;  // Nothing to do.

  // For now, we ignore options.capacity_num_bytes as a total of all pending
  // writes (and just treat it per message). We will implement that later if
  // we need to. All current uses want all their data to be sent, and it's not
  // clear that this backpressure should be done at the mojo layer or at a
  // higher application layer.
  bool all_or_none = flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE;
  uint32_t min_num_bytes_to_write = all_or_none ? *num_bytes : 0;
  if (min_num_bytes_to_write > options_.capacity_num_bytes) {
    // Don't return "should wait" since you can't wait for a specified amount of
    // data.
    return MOJO_RESULT_OUT_OF_RANGE;
  }

  uint32_t num_bytes_to_write =
      std::min(*num_bytes, options_.capacity_num_bytes);
  if (num_bytes_to_write == 0)
    return MOJO_RESULT_SHOULD_WAIT;

  HandleSignalsState old_state = GetHandleSignalsStateImplNoLock();

  *num_bytes = num_bytes_to_write;
  WriteDataIntoMessages(elements, num_bytes_to_write);

  HandleSignalsState new_state = GetHandleSignalsStateImplNoLock();
  if (!new_state.equals(old_state))
    awakable_list_.AwakeForStateChange(new_state);
  return MOJO_RESULT_OK;
}

MojoResult DataPipeProducerDispatcher::BeginWriteDataImplNoLock(
    void** buffer,
    uint32_t* buffer_num_bytes,
    MojoWriteDataFlags flags) {
  lock().AssertAcquired();
  if (InTwoPhaseWrite())
    return MOJO_RESULT_BUSY;
  if (error_)
    return MOJO_RESULT_FAILED_PRECONDITION;

  bool all_or_none = flags & MOJO_WRITE_DATA_FLAG_ALL_OR_NONE;
  uint32_t min_num_bytes_to_write = 0;
  if (all_or_none) {
    min_num_bytes_to_write = *buffer_num_bytes;
    if (min_num_bytes_to_write % options_.element_num_bytes != 0)
      return MOJO_RESULT_INVALID_ARGUMENT;
    if (min_num_bytes_to_write > options_.capacity_num_bytes) {
      // Don't return "should wait" since you can't wait for a specified amount
      // of data.
      return MOJO_RESULT_OUT_OF_RANGE;
    }
  }

  // See comment in WriteDataImplNoLock about ignoring capacity_num_bytes.
  if (*buffer_num_bytes == 0)
    *buffer_num_bytes = options_.capacity_num_bytes;

  two_phase_data_.resize(*buffer_num_bytes);
  *buffer = &two_phase_data_[0];

  // TODO: if buffer_num_bytes.Get() > GetConfiguration().max_message_num_bytes
  // we can construct a MessageInTransit here. But then we need to make
  // MessageInTransit support changing its data size later.

  return MOJO_RESULT_OK;
}

MojoResult DataPipeProducerDispatcher::EndWriteDataImplNoLock(
    uint32_t num_bytes_written) {
  lock().AssertAcquired();
  if (!InTwoPhaseWrite())
    return MOJO_RESULT_FAILED_PRECONDITION;

  // Note: Allow successful completion of the two-phase write even if the other
  // side has been closed.
  MojoResult rv = MOJO_RESULT_OK;
  if (num_bytes_written > two_phase_data_.size() ||
      num_bytes_written % options_.element_num_bytes != 0) {
    rv = MOJO_RESULT_INVALID_ARGUMENT;
  } else if (channel_) {
    WriteDataIntoMessages(&two_phase_data_[0], num_bytes_written);
  }

  // Two-phase write ended even on failure.
  two_phase_data_.clear();
  // If we're now writable, we *became* writable (since we weren't writable
  // during the two-phase write), so awake producer awakables.
  HandleSignalsState new_state = GetHandleSignalsStateImplNoLock();
  if (new_state.satisfies(MOJO_HANDLE_SIGNAL_WRITABLE))
    awakable_list_.AwakeForStateChange(new_state);

  return rv;
}

HandleSignalsState DataPipeProducerDispatcher::GetHandleSignalsStateImplNoLock()
    const {
  lock().AssertAcquired();

  HandleSignalsState rv;
  if (!error_) {
    if (!InTwoPhaseWrite())
      rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
    rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_WRITABLE;
  } else {
    rv.satisfied_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  }
  rv.satisfiable_signals |= MOJO_HANDLE_SIGNAL_PEER_CLOSED;
  return rv;
}

MojoResult DataPipeProducerDispatcher::AddAwakableImplNoLock(
    Awakable* awakable,
    MojoHandleSignals signals,
    uint32_t context,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  HandleSignalsState state = GetHandleSignalsStateImplNoLock();
  if (state.satisfies(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_ALREADY_EXISTS;
  }
  if (!state.can_satisfy(signals)) {
    if (signals_state)
      *signals_state = state;
    return MOJO_RESULT_FAILED_PRECONDITION;
  }

  awakable_list_.Add(awakable, signals, context);
  return MOJO_RESULT_OK;
}

void DataPipeProducerDispatcher::RemoveAwakableImplNoLock(
    Awakable* awakable,
    HandleSignalsState* signals_state) {
  lock().AssertAcquired();
  awakable_list_.Remove(awakable);
  if (signals_state)
    *signals_state = GetHandleSignalsStateImplNoLock();
}

void DataPipeProducerDispatcher::StartSerializeImplNoLock(
    size_t* max_size,
    size_t* max_platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.

  if (channel_) {
    std::vector<char> temp;
    serialized_platform_handle_ = channel_->ReleaseHandle(&temp);
    channel_ = nullptr;
    DCHECK(temp.empty());
  }
  DataPipe::StartSerialize(serialized_platform_handle_.is_valid(),
                           false, max_size, max_platform_handles);
}

bool DataPipeProducerDispatcher::EndSerializeAndCloseImplNoLock(
    void* destination,
    size_t* actual_size,
    PlatformHandleVector* platform_handles) {
  DCHECK(HasOneRef());  // Only one ref => no need to take the lock.

  DataPipe::EndSerialize(
      options_,
      serialized_platform_handle_.Pass(),
      ScopedPlatformHandle(), 0,
      destination, actual_size, platform_handles);
  CloseImplNoLock();
  return true;
}

void DataPipeProducerDispatcher::TransportStarted() {
  started_transport_.Acquire();
}

void DataPipeProducerDispatcher::TransportEnded() {
  started_transport_.Release();
}

bool DataPipeProducerDispatcher::IsBusyNoLock() const {
  lock().AssertAcquired();
  return InTwoPhaseWrite();
}

void DataPipeProducerDispatcher::OnReadMessage(
    const MessageInTransit::View& message_view,
    ScopedPlatformHandleVectorPtr platform_handles) {
  NOTREACHED();
}

void DataPipeProducerDispatcher::OnError(Error error) {
  switch (error) {
    case ERROR_READ_SHUTDOWN:
    case ERROR_READ_BROKEN:
    case ERROR_READ_BAD_MESSAGE:
    case ERROR_READ_UNKNOWN:
      LOG(ERROR) << "DataPipeProducerDispatcher shouldn't read messages";
      break;
    case ERROR_WRITE:
      // Write errors are slightly notable: they probably shouldn't happen under
      // normal operation (but maybe the other side crashed).
      LOG(WARNING) << "DataPipeProducerDispatcher write error";
      break;
  }

  error_ = true;
  if (started_transport_.Try()) {
    base::AutoLock locker(lock());
    awakable_list_.AwakeForStateChange(GetHandleSignalsStateImplNoLock());

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RawChannel::Shutdown, base::Unretained(channel_)));
    channel_ = nullptr;
    started_transport_.Release();
  } else {
    // We must be waiting to call ReleaseHandle. It will call Shutdown.
  }
}

bool DataPipeProducerDispatcher::InTwoPhaseWrite() const {
  return !two_phase_data_.empty();
}

bool DataPipeProducerDispatcher::WriteDataIntoMessages(
    const void* elements,
    uint32_t num_bytes) {
  // The maximum amount of data to send per message (make it a multiple of the
  // element size.
  size_t max_message_num_bytes = GetConfiguration().max_message_num_bytes;
  max_message_num_bytes -= max_message_num_bytes % options_.element_num_bytes;
  DCHECK_GT(max_message_num_bytes, 0u);

  uint32_t offset = 0;
  while (offset < num_bytes) {
    uint32_t message_num_bytes =
        std::min(static_cast<uint32_t>(max_message_num_bytes),
                 num_bytes - offset);
    scoped_ptr<MessageInTransit> message(new MessageInTransit(
        MessageInTransit::Type::MESSAGE, message_num_bytes,
        static_cast<const char*>(elements) + offset));
    if (!channel_->WriteMessage(message.Pass())) {
      error_ = true;
      return false;
    }

    offset += message_num_bytes;
  }

  return true;
}

}  // namespace edk
}  // namespace mojo
