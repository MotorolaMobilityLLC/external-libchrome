// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/gles2/command_buffer_client_impl.h"

#include <limits>

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/public/bindings/sync_dispatcher.h"
#include "mojo/services/gles2/command_buffer_type_conversions.h"

namespace mojo {
namespace gles2 {

CommandBufferDelegate::~CommandBufferDelegate() {}

void CommandBufferDelegate::ContextLost() {}
void CommandBufferDelegate::DrawAnimationFrame() {}

CommandBufferClientImpl::CommandBufferClientImpl(
    CommandBufferDelegate* delegate,
    MojoAsyncWaiter* async_waiter,
    ScopedCommandBufferHandle command_buffer_handle)
    : delegate_(delegate),
      command_buffer_(command_buffer_handle.Pass(), this, this, async_waiter),
      last_put_offset_(-1),
      next_transfer_buffer_id_(0),
      initialize_result_(false) {}

CommandBufferClientImpl::~CommandBufferClientImpl() {}

bool CommandBufferClientImpl::Initialize() {
  shared_state_shm_.reset(new base::SharedMemory);
  if (!shared_state_shm_->CreateAndMapAnonymous(
           sizeof(gpu::CommandBufferSharedState)))
    return false;

  base::SharedMemoryHandle handle;
  shared_state_shm_->ShareToProcess(base::GetCurrentProcessHandle(), &handle);
  if (!base::SharedMemory::IsHandleValid(handle))
    return false;

  shared_state()->Initialize();

  InterfacePipe<CommandBufferSyncClient, NoInterface> sync_pipe;
  sync_dispatcher_.reset(new SyncDispatcher<CommandBufferSyncClient>(
      sync_pipe.handle_to_peer.Pass(), this));
  AllocationScope scope;
  command_buffer_->Initialize(sync_pipe.handle_to_self.Pass(), handle);
  // Wait for DidInitialize to come on the sync client pipe.
  if (!sync_dispatcher_->WaitAndDispatchOneMessage()) {
    VLOG(1) << "Channel encountered error while creating command buffer";
    return false;
  }
  return initialize_result_;
}

gpu::CommandBuffer::State CommandBufferClientImpl::GetState() {
  MakeProgressAndUpdateState();
  return last_state_;
}

gpu::CommandBuffer::State CommandBufferClientImpl::GetLastState() {
  return last_state_;
}

int32 CommandBufferClientImpl::GetLastToken() {
  TryUpdateState();
  return last_state_.token;
}

void CommandBufferClientImpl::Flush(int32 put_offset) {
  if (last_put_offset_ == put_offset)
    return;

  last_put_offset_ = put_offset;
  command_buffer_->Flush(put_offset);
}

gpu::CommandBuffer::State CommandBufferClientImpl::FlushSync(
    int32 put_offset,
    int32 last_known_get) {
  Flush(put_offset);
  TryUpdateState();
  if (last_known_get == last_state_.get_offset)
    MakeProgressAndUpdateState();

  return last_state_;
}

void CommandBufferClientImpl::SetGetBuffer(int32 shm_id) {
  command_buffer_->SetGetBuffer(shm_id);
  last_put_offset_ = -1;
}

void CommandBufferClientImpl::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

gpu::Buffer CommandBufferClientImpl::CreateTransferBuffer(size_t size,
                                                          int32* id) {
  gpu::Buffer buffer;
  if (size >= std::numeric_limits<uint32_t>::max())
    return buffer;

  scoped_ptr<base::SharedMemory> shared_memory(new base::SharedMemory);
  if (!shared_memory->CreateAndMapAnonymous(size))
    return buffer;

  base::SharedMemoryHandle handle;
  shared_memory->ShareToProcess(base::GetCurrentProcessHandle(), &handle);
  if (!base::SharedMemory::IsHandleValid(handle))
    return buffer;

  *id = ++next_transfer_buffer_id_;
  DCHECK(transfer_buffers_.find(*id) == transfer_buffers_.end());

  AllocationScope scope;
  command_buffer_->RegisterTransferBuffer(
      *id, handle, static_cast<uint32_t>(size));

  buffer.ptr = shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = shared_memory.release();
  transfer_buffers_[*id] = buffer;

  return buffer;
}

void CommandBufferClientImpl::DestroyTransferBuffer(int32 id) {
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end()) {
    delete it->second.shared_memory;
    transfer_buffers_.erase(it);
  }
  command_buffer_->DestroyTransferBuffer(id);
}

gpu::Buffer CommandBufferClientImpl::GetTransferBuffer(int32 id) {
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end()) {
    return it->second;
  } else {
    return gpu::Buffer();
  }
}

void CommandBufferClientImpl::SetToken(int32 token) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferClientImpl::SetParseError(gpu::error::Error error) {
  // Not implemented in proxy.
  NOTREACHED();
}

void CommandBufferClientImpl::SetContextLostReason(
    gpu::error::ContextLostReason reason) {
  // Not implemented in proxy.
  NOTREACHED();
}

gpu::Capabilities CommandBufferClientImpl::GetCapabilities() {
  // TODO(piman)
  NOTIMPLEMENTED();
  return gpu::Capabilities();
}

gfx::GpuMemoryBuffer* CommandBufferClientImpl::CreateGpuMemoryBuffer(
    size_t width,
    size_t height,
    unsigned internalformat,
    int32* id) {
  // TODO(piman)
  NOTIMPLEMENTED();
  return NULL;
}

void CommandBufferClientImpl::DestroyGpuMemoryBuffer(int32 id) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

uint32 CommandBufferClientImpl::InsertSyncPoint() {
  // TODO(piman)
  NOTIMPLEMENTED();
  return 0;
}

void CommandBufferClientImpl::SignalSyncPoint(uint32 sync_point,
                                              const base::Closure& callback) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

void CommandBufferClientImpl::SignalQuery(uint32 query,
                                          const base::Closure& callback) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

void CommandBufferClientImpl::SetSurfaceVisible(bool visible) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

void CommandBufferClientImpl::SendManagedMemoryStats(
    const gpu::ManagedMemoryStats& stats) {
  // TODO(piman)
  NOTIMPLEMENTED();
}

void CommandBufferClientImpl::Echo(const base::Closure& callback) {
  echo_closures_.push(callback);
  command_buffer_->Echo();
}

uint32 CommandBufferClientImpl::CreateStreamTexture(uint32 texture_id) {
  // TODO(piman)
  NOTIMPLEMENTED();
  return 0;
}

void CommandBufferClientImpl::RequestAnimationFrames() {
  command_buffer_->RequestAnimationFrames();
}

void CommandBufferClientImpl::CancelAnimationFrames() {
  command_buffer_->CancelAnimationFrames();
}

void CommandBufferClientImpl::DidInitialize(bool success) {
  initialize_result_ = success;
}

void CommandBufferClientImpl::DidMakeProgress(const CommandBufferState& state) {
  if (state.generation() - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

void CommandBufferClientImpl::DidDestroy() {
  LostContext(gpu::error::kUnknown);
}

void CommandBufferClientImpl::EchoAck() {
  base::Closure closure = echo_closures_.front();
  echo_closures_.pop();
  closure.Run();
}

void CommandBufferClientImpl::LostContext(int32_t lost_reason) {
  last_state_.error = gpu::error::kLostContext;
  last_state_.context_lost_reason =
      static_cast<gpu::error::ContextLostReason>(lost_reason);
  delegate_->ContextLost();
}

void CommandBufferClientImpl::OnError() { LostContext(gpu::error::kUnknown); }

void CommandBufferClientImpl::TryUpdateState() {
  if (last_state_.error == gpu::error::kNoError)
    shared_state()->Read(&last_state_);
}

void CommandBufferClientImpl::MakeProgressAndUpdateState() {
  command_buffer_->MakeProgress(last_state_.get_offset);
  if (!sync_dispatcher_->WaitAndDispatchOneMessage()) {
    VLOG(1) << "Channel encountered error while waiting for command buffer";
    // TODO(piman): is it ok for this to re-enter?
    DidDestroy();
    return;
  }
}

void CommandBufferClientImpl::DrawAnimationFrame() {
  delegate_->DrawAnimationFrame();
}

}  // namespace gles2
}  // namespace mojo
