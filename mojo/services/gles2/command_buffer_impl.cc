// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/gles2/command_buffer_impl.h"

#include "base/bind.h"
#include "base/memory/shared_memory.h"

#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/command_buffer_service.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "gpu/command_buffer/service/gpu_control_service.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "mojo/public/bindings/allocation_scope.h"
#include "mojo/services/gles2/command_buffer_type_conversions.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace mojo {
namespace services {

namespace {

class MemoryTrackerStub : public gpu::gles2::MemoryTracker {
 public:
  MemoryTrackerStub() {}

  virtual void TrackMemoryAllocatedChange(size_t old_size,
                                          size_t new_size,
                                          gpu::gles2::MemoryTracker::Pool pool)
      OVERRIDE {}

  virtual bool EnsureGPUMemoryAvailable(size_t size_needed) OVERRIDE {
    return true;
  };

 private:
  virtual ~MemoryTrackerStub() {}

  DISALLOW_COPY_AND_ASSIGN(MemoryTrackerStub);
};

}  // anonymous namespace

CommandBufferImpl::CommandBufferImpl(ScopedCommandBufferClientHandle client,
                                     gfx::AcceleratedWidget widget,
                                     const gfx::Size& size)
    : client_(client.Pass(), this), widget_(widget), size_(size) {}

CommandBufferImpl::~CommandBufferImpl() { client_->DidDestroy(); }

void CommandBufferImpl::Initialize(
    ScopedCommandBufferSyncClientHandle sync_client,
    const ShmHandle& shared_state) {
  sync_client_.reset(sync_client.Pass());
  sync_client_->DidInitialize(DoInitialize(shared_state));
}

bool CommandBufferImpl::DoInitialize(const ShmHandle& shared_state) {
  // TODO(piman): offscreen surface.
  scoped_refptr<gfx::GLSurface> surface =
      gfx::GLSurface::CreateViewGLSurface(widget_);
  if (!surface.get())
    return false;

  // TODO(piman): context sharing, virtual contexts, gpu preference.
  scoped_refptr<gfx::GLContext> context = gfx::GLContext::CreateGLContext(
      NULL, surface.get(), gfx::PreferIntegratedGpu);
  if (!context.get())
    return false;

  if (!context->MakeCurrent(surface.get()))
    return false;

  scoped_refptr<gpu::gles2::ContextGroup> context_group =
      new gpu::gles2::ContextGroup(
          NULL, NULL, new MemoryTrackerStub(), NULL, true);
  command_buffer_.reset(
      new gpu::CommandBufferService(context_group->transfer_buffer_manager()));
  bool result = command_buffer_->Initialize();
  DCHECK(result);

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group.get()));
  scheduler_.reset(new gpu::GpuScheduler(
      command_buffer_.get(), decoder_.get(), decoder_.get()));
  decoder_->set_engine(scheduler_.get());

  gpu::gles2::DisallowedFeatures disallowed_features;

  // TODO(piman): attributes.
  std::vector<int32> attrib_vector;
  if (!decoder_->Initialize(surface,
                            context,
                            false /* offscreen */,
                            size_,
                            disallowed_features,
                            attrib_vector))
    return false;

  gpu_control_.reset(
      new gpu::GpuControlService(context_group->image_manager(),
                                 NULL,
                                 context_group->mailbox_manager(),
                                 NULL,
                                 decoder_->GetCapabilities()));

  command_buffer_->SetPutOffsetChangeCallback(base::Bind(
      &gpu::GpuScheduler::PutChanged, base::Unretained(scheduler_.get())));
  command_buffer_->SetGetBufferChangeCallback(base::Bind(
      &gpu::GpuScheduler::SetGetBuffer, base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&CommandBufferImpl::OnParseError, base::Unretained(this)));

  // TODO(piman): other callbacks

  scoped_ptr<base::SharedMemory> shared_state_shm(
      new base::SharedMemory(shared_state, false));
  if (!command_buffer_->SetSharedStateBuffer(shared_state_shm.Pass()))
    return false;

  return true;
}

void CommandBufferImpl::SetGetBuffer(int32_t buffer) {
  command_buffer_->SetGetBuffer(buffer);
}

void CommandBufferImpl::Flush(int32_t put_offset) {
  command_buffer_->Flush(put_offset);
}

void CommandBufferImpl::MakeProgress(int32_t last_get_offset) {
  // TODO(piman): handle out-of-order.
  AllocationScope scope;
  sync_client_->DidMakeProgress(command_buffer_->GetState());
}

void CommandBufferImpl::RegisterTransferBuffer(int32_t id,
                                               const ShmHandle& transfer_buffer,
                                               uint32_t size) {
  bool read_only = false;
  base::SharedMemory shared_memory(transfer_buffer, read_only);
  command_buffer_->RegisterTransferBuffer(id, &shared_memory, size);
}

void CommandBufferImpl::DestroyTransferBuffer(int32_t id) {
  command_buffer_->DestroyTransferBuffer(id);
}

void CommandBufferImpl::Echo() { client_->EchoAck(); }

void CommandBufferImpl::RequestAnimationFrames() {
  timer_.Start(FROM_HERE,
               base::TimeDelta::FromMilliseconds(16),
               this,
               &CommandBufferImpl::DrawAnimationFrame);
}

void CommandBufferImpl::CancelAnimationFrames() { timer_.Stop(); }

void CommandBufferImpl::OnParseError() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  client_->LostContext(state.context_lost_reason);
}

void CommandBufferImpl::DrawAnimationFrame() { client_->DrawAnimationFrame(); }

}  // namespace services
}  // namespace mojo
