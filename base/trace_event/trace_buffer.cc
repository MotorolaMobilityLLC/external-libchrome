// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_buffer.h"

#include "base/trace_event/trace_event_impl.h"

namespace base {
namespace trace_event {

namespace {

class TraceBufferRingBuffer : public TraceBuffer {
 public:
  TraceBufferRingBuffer(size_t max_chunks)
      : max_chunks_(max_chunks),
        recyclable_chunks_queue_(new size_t[queue_capacity()]),
        queue_head_(0),
        queue_tail_(max_chunks),
        current_iteration_index_(0),
        current_chunk_seq_(1) {
    chunks_.reserve(max_chunks);
    for (size_t i = 0; i < max_chunks; ++i)
      recyclable_chunks_queue_[i] = i;
  }

  scoped_ptr<TraceBufferChunk> GetChunk(size_t* index) override {
    // Because the number of threads is much less than the number of chunks,
    // the queue should never be empty.
    DCHECK(!QueueIsEmpty());

    *index = recyclable_chunks_queue_[queue_head_];
    queue_head_ = NextQueueIndex(queue_head_);
    current_iteration_index_ = queue_head_;

    if (*index >= chunks_.size())
      chunks_.resize(*index + 1);

    TraceBufferChunk* chunk = chunks_[*index];
    chunks_[*index] = NULL;  // Put NULL in the slot of a in-flight chunk.
    if (chunk)
      chunk->Reset(current_chunk_seq_++);
    else
      chunk = new TraceBufferChunk(current_chunk_seq_++);

    return scoped_ptr<TraceBufferChunk>(chunk);
  }

  void ReturnChunk(size_t index, scoped_ptr<TraceBufferChunk> chunk) override {
    // When this method is called, the queue should not be full because it
    // can contain all chunks including the one to be returned.
    DCHECK(!QueueIsFull());
    DCHECK(chunk);
    DCHECK_LT(index, chunks_.size());
    DCHECK(!chunks_[index]);
    chunks_[index] = chunk.release();
    recyclable_chunks_queue_[queue_tail_] = index;
    queue_tail_ = NextQueueIndex(queue_tail_);
  }

  bool IsFull() const override { return false; }

  size_t Size() const override {
    // This is approximate because not all of the chunks are full.
    return chunks_.size() * TraceBufferChunk::kTraceBufferChunkSize;
  }

  size_t Capacity() const override {
    return max_chunks_ * TraceBufferChunk::kTraceBufferChunkSize;
  }

  TraceEvent* GetEventByHandle(TraceEventHandle handle) override {
    if (handle.chunk_index >= chunks_.size())
      return NULL;
    TraceBufferChunk* chunk = chunks_[handle.chunk_index];
    if (!chunk || chunk->seq() != handle.chunk_seq)
      return NULL;
    return chunk->GetEventAt(handle.event_index);
  }

  const TraceBufferChunk* NextChunk() override {
    if (chunks_.empty())
      return NULL;

    while (current_iteration_index_ != queue_tail_) {
      size_t chunk_index = recyclable_chunks_queue_[current_iteration_index_];
      current_iteration_index_ = NextQueueIndex(current_iteration_index_);
      if (chunk_index >= chunks_.size())  // Skip uninitialized chunks.
        continue;
      DCHECK(chunks_[chunk_index]);
      return chunks_[chunk_index];
    }
    return NULL;
  }

  scoped_ptr<TraceBuffer> CloneForIteration() const override {
    scoped_ptr<ClonedTraceBuffer> cloned_buffer(new ClonedTraceBuffer());
    for (size_t queue_index = queue_head_; queue_index != queue_tail_;
         queue_index = NextQueueIndex(queue_index)) {
      size_t chunk_index = recyclable_chunks_queue_[queue_index];
      if (chunk_index >= chunks_.size())  // Skip uninitialized chunks.
        continue;
      TraceBufferChunk* chunk = chunks_[chunk_index];
      cloned_buffer->chunks_.push_back(chunk ? chunk->Clone().release() : NULL);
    }
    return cloned_buffer.Pass();
  }

  void EstimateTraceMemoryOverhead(
      TraceEventMemoryOverhead* overhead) override {
    overhead->Add("TraceBufferRingBuffer", sizeof(*this));
    for (size_t queue_index = queue_head_; queue_index != queue_tail_;
         queue_index = NextQueueIndex(queue_index)) {
      size_t chunk_index = recyclable_chunks_queue_[queue_index];
      if (chunk_index >= chunks_.size())  // Skip uninitialized chunks.
        continue;
      chunks_[chunk_index]->EstimateTraceMemoryOverhead(overhead);
    }
  }

 private:
  class ClonedTraceBuffer : public TraceBuffer {
   public:
    ClonedTraceBuffer() : current_iteration_index_(0) {}

    // The only implemented method.
    const TraceBufferChunk* NextChunk() override {
      return current_iteration_index_ < chunks_.size()
                 ? chunks_[current_iteration_index_++]
                 : NULL;
    }

    scoped_ptr<TraceBufferChunk> GetChunk(size_t* index) override {
      NOTIMPLEMENTED();
      return scoped_ptr<TraceBufferChunk>();
    }
    void ReturnChunk(size_t index, scoped_ptr<TraceBufferChunk>) override {
      NOTIMPLEMENTED();
    }
    bool IsFull() const override { return false; }
    size_t Size() const override { return 0; }
    size_t Capacity() const override { return 0; }
    TraceEvent* GetEventByHandle(TraceEventHandle handle) override {
      return NULL;
    }
    scoped_ptr<TraceBuffer> CloneForIteration() const override {
      NOTIMPLEMENTED();
      return scoped_ptr<TraceBuffer>();
    }
    void EstimateTraceMemoryOverhead(
        TraceEventMemoryOverhead* overhead) override {
      NOTIMPLEMENTED();
    }

    size_t current_iteration_index_;
    ScopedVector<TraceBufferChunk> chunks_;
  };

  bool QueueIsEmpty() const { return queue_head_ == queue_tail_; }

  size_t QueueSize() const {
    return queue_tail_ > queue_head_
               ? queue_tail_ - queue_head_
               : queue_tail_ + queue_capacity() - queue_head_;
  }

  bool QueueIsFull() const { return QueueSize() == queue_capacity() - 1; }

  size_t queue_capacity() const {
    // One extra space to help distinguish full state and empty state.
    return max_chunks_ + 1;
  }

  size_t NextQueueIndex(size_t index) const {
    index++;
    if (index >= queue_capacity())
      index = 0;
    return index;
  }

  size_t max_chunks_;
  ScopedVector<TraceBufferChunk> chunks_;

  scoped_ptr<size_t[]> recyclable_chunks_queue_;
  size_t queue_head_;
  size_t queue_tail_;

  size_t current_iteration_index_;
  uint32 current_chunk_seq_;

  DISALLOW_COPY_AND_ASSIGN(TraceBufferRingBuffer);
};

class TraceBufferVector : public TraceBuffer {
 public:
  TraceBufferVector(size_t max_chunks)
      : in_flight_chunk_count_(0),
        current_iteration_index_(0),
        max_chunks_(max_chunks) {
    chunks_.reserve(max_chunks_);
  }

  scoped_ptr<TraceBufferChunk> GetChunk(size_t* index) override {
    // This function may be called when adding normal events or indirectly from
    // AddMetadataEventsWhileLocked(). We can not DECHECK(!IsFull()) because we
    // have to add the metadata events and flush thread-local buffers even if
    // the buffer is full.
    *index = chunks_.size();
    chunks_.push_back(NULL);  // Put NULL in the slot of a in-flight chunk.
    ++in_flight_chunk_count_;
    // + 1 because zero chunk_seq is not allowed.
    return scoped_ptr<TraceBufferChunk>(
        new TraceBufferChunk(static_cast<uint32>(*index) + 1));
  }

  void ReturnChunk(size_t index, scoped_ptr<TraceBufferChunk> chunk) override {
    DCHECK_GT(in_flight_chunk_count_, 0u);
    DCHECK_LT(index, chunks_.size());
    DCHECK(!chunks_[index]);
    --in_flight_chunk_count_;
    chunks_[index] = chunk.release();
  }

  bool IsFull() const override { return chunks_.size() >= max_chunks_; }

  size_t Size() const override {
    // This is approximate because not all of the chunks are full.
    return chunks_.size() * TraceBufferChunk::kTraceBufferChunkSize;
  }

  size_t Capacity() const override {
    return max_chunks_ * TraceBufferChunk::kTraceBufferChunkSize;
  }

  TraceEvent* GetEventByHandle(TraceEventHandle handle) override {
    if (handle.chunk_index >= chunks_.size())
      return NULL;
    TraceBufferChunk* chunk = chunks_[handle.chunk_index];
    if (!chunk || chunk->seq() != handle.chunk_seq)
      return NULL;
    return chunk->GetEventAt(handle.event_index);
  }

  const TraceBufferChunk* NextChunk() override {
    while (current_iteration_index_ < chunks_.size()) {
      // Skip in-flight chunks.
      const TraceBufferChunk* chunk = chunks_[current_iteration_index_++];
      if (chunk)
        return chunk;
    }
    return NULL;
  }

  scoped_ptr<TraceBuffer> CloneForIteration() const override {
    NOTIMPLEMENTED();
    return scoped_ptr<TraceBuffer>();
  }

  void EstimateTraceMemoryOverhead(
      TraceEventMemoryOverhead* overhead) override {
    const size_t chunks_ptr_vector_allocated_size =
        sizeof(*this) + max_chunks_ * sizeof(decltype(chunks_)::value_type);
    const size_t chunks_ptr_vector_resident_size =
        sizeof(*this) + chunks_.size() * sizeof(decltype(chunks_)::value_type);
    overhead->Add("TraceBufferVector", chunks_ptr_vector_allocated_size,
                  chunks_ptr_vector_resident_size);
    for (size_t i = 0; i < chunks_.size(); ++i) {
      TraceBufferChunk* chunk = chunks_[i];
      // Skip the in-flight (nullptr) chunks. They will be accounted by the
      // per-thread-local dumpers, see ThreadLocalEventBuffer::OnMemoryDump.
      if (chunk)
        chunk->EstimateTraceMemoryOverhead(overhead);
    }
  }

 private:
  size_t in_flight_chunk_count_;
  size_t current_iteration_index_;
  size_t max_chunks_;
  ScopedVector<TraceBufferChunk> chunks_;

  DISALLOW_COPY_AND_ASSIGN(TraceBufferVector);
};

}  // namespace

TraceBufferChunk::TraceBufferChunk(uint32 seq) : next_free_(0), seq_(seq) {}

TraceBufferChunk::~TraceBufferChunk() {}

void TraceBufferChunk::Reset(uint32 new_seq) {
  for (size_t i = 0; i < next_free_; ++i)
    chunk_[i].Reset();
  next_free_ = 0;
  seq_ = new_seq;
  cached_overhead_estimate_when_full_.reset();
}

TraceEvent* TraceBufferChunk::AddTraceEvent(size_t* event_index) {
  DCHECK(!IsFull());
  *event_index = next_free_++;
  return &chunk_[*event_index];
}

scoped_ptr<TraceBufferChunk> TraceBufferChunk::Clone() const {
  scoped_ptr<TraceBufferChunk> cloned_chunk(new TraceBufferChunk(seq_));
  cloned_chunk->next_free_ = next_free_;
  for (size_t i = 0; i < next_free_; ++i)
    cloned_chunk->chunk_[i].CopyFrom(chunk_[i]);
  return cloned_chunk.Pass();
}

void TraceBufferChunk::EstimateTraceMemoryOverhead(
    TraceEventMemoryOverhead* overhead) {
  if (cached_overhead_estimate_when_full_) {
    DCHECK(IsFull());
    overhead->Update(*cached_overhead_estimate_when_full_);
    return;
  }

  // Cache the memory overhead estimate only if the chunk is full.
  TraceEventMemoryOverhead* estimate = overhead;
  if (IsFull()) {
    cached_overhead_estimate_when_full_.reset(new TraceEventMemoryOverhead);
    estimate = cached_overhead_estimate_when_full_.get();
  }

  estimate->Add("TraceBufferChunk", sizeof(*this));
  for (size_t i = 0; i < next_free_; ++i)
    chunk_[i].EstimateTraceMemoryOverhead(estimate);

  if (IsFull()) {
    estimate->AddSelf();
    overhead->Update(*estimate);
  }
}

TraceResultBuffer::OutputCallback
TraceResultBuffer::SimpleOutput::GetCallback() {
  return Bind(&SimpleOutput::Append, Unretained(this));
}

void TraceResultBuffer::SimpleOutput::Append(
    const std::string& json_trace_output) {
  json_output += json_trace_output;
}

TraceResultBuffer::TraceResultBuffer() : append_comma_(false) {}

TraceResultBuffer::~TraceResultBuffer() {}

void TraceResultBuffer::SetOutputCallback(
    const OutputCallback& json_chunk_callback) {
  output_callback_ = json_chunk_callback;
}

void TraceResultBuffer::Start() {
  append_comma_ = false;
  output_callback_.Run("[");
}

void TraceResultBuffer::AddFragment(const std::string& trace_fragment) {
  if (append_comma_)
    output_callback_.Run(",");
  append_comma_ = true;
  output_callback_.Run(trace_fragment);
}

void TraceResultBuffer::Finish() {
  output_callback_.Run("]");
}

TraceBuffer* TraceBuffer::CreateTraceBufferRingBuffer(size_t max_chunks) {
  return new TraceBufferRingBuffer(max_chunks);
}

TraceBuffer* TraceBuffer::CreateTraceBufferVectorOfSize(size_t max_chunks) {
  return new TraceBufferVector(max_chunks);
}

}  // namespace trace_event
}  // namespace base
