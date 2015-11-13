// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// WARNING this file was generated by generate_nacl_bindings.py
// Do not edit by hand.

#ifndef MOJO_PUBLIC_PLATFORM_NACL_MOJO_IRT_H_
#define MOJO_PUBLIC_PLATFORM_NACL_MOJO_IRT_H_

#include "mojo/public/c/system/buffer.h"
#include "mojo/public/c/system/data_pipe.h"
#include "mojo/public/c/system/message_pipe.h"
#include "mojo/public/c/system/types.h"

#define NACL_IRT_MOJO_v0_1 "nacl-irt-mojo-0.1"

struct nacl_irt_mojo {
  MojoResult (*MojoCreateSharedBuffer)(
      const struct MojoCreateSharedBufferOptions* options,
      uint64_t num_bytes,
      MojoHandle* shared_buffer_handle);
  MojoResult (*MojoDuplicateBufferHandle)(
      MojoHandle buffer_handle,
      const struct MojoDuplicateBufferHandleOptions* options,
      MojoHandle* new_buffer_handle);
  MojoResult (*MojoMapBuffer)(MojoHandle buffer_handle,
                              uint64_t offset,
                              uint64_t num_bytes,
                              void** buffer,
                              MojoMapBufferFlags flags);
  MojoResult (*MojoUnmapBuffer)(void* buffer);
  MojoResult (*MojoCreateDataPipe)(
      const struct MojoCreateDataPipeOptions* options,
      MojoHandle* data_pipe_producer_handle,
      MojoHandle* data_pipe_consumer_handle);
  MojoResult (*MojoWriteData)(MojoHandle data_pipe_producer_handle,
                              const void* elements,
                              uint32_t* num_bytes,
                              MojoWriteDataFlags flags);
  MojoResult (*MojoBeginWriteData)(MojoHandle data_pipe_producer_handle,
                                   void** buffer,
                                   uint32_t* buffer_num_bytes,
                                   MojoWriteDataFlags flags);
  MojoResult (*MojoEndWriteData)(MojoHandle data_pipe_producer_handle,
                                 uint32_t num_bytes_written);
  MojoResult (*MojoReadData)(MojoHandle data_pipe_consumer_handle,
                             void* elements,
                             uint32_t* num_bytes,
                             MojoReadDataFlags flags);
  MojoResult (*MojoBeginReadData)(MojoHandle data_pipe_consumer_handle,
                                  const void** buffer,
                                  uint32_t* buffer_num_bytes,
                                  MojoReadDataFlags flags);
  MojoResult (*MojoEndReadData)(MojoHandle data_pipe_consumer_handle,
                                uint32_t num_bytes_read);
  MojoTimeTicks (*MojoGetTimeTicksNow)();
  MojoResult (*MojoClose)(MojoHandle handle);
  MojoResult (*MojoWait)(MojoHandle handle,
                         MojoHandleSignals signals,
                         MojoDeadline deadline,
                         struct MojoHandleSignalsState* signals_state);
  MojoResult (*MojoWaitMany)(const MojoHandle* handles,
                             const MojoHandleSignals* signals,
                             uint32_t num_handles,
                             MojoDeadline deadline,
                             uint32_t* result_index,
                             struct MojoHandleSignalsState* signals_states);
  MojoResult (*MojoCreateMessagePipe)(
      const struct MojoCreateMessagePipeOptions* options,
      MojoHandle* message_pipe_handle0,
      MojoHandle* message_pipe_handle1);
  MojoResult (*MojoWriteMessage)(MojoHandle message_pipe_handle,
                                 const void* bytes,
                                 uint32_t num_bytes,
                                 const MojoHandle* handles,
                                 uint32_t num_handles,
                                 MojoWriteMessageFlags flags);
  MojoResult (*MojoReadMessage)(MojoHandle message_pipe_handle,
                                void* bytes,
                                uint32_t* num_bytes,
                                MojoHandle* handles,
                                uint32_t* num_handles,
                                MojoReadMessageFlags flags);
  MojoResult (*_MojoGetInitialHandle)(MojoHandle* handle);
};

#ifdef __cplusplus
extern "C" {
#endif

size_t mojo_irt_query(const char* interface_ident,
                      void* table,
                      size_t tablesize);

#ifdef __cplusplus
}
#endif

#endif  // MOJO_PUBLIC_PLATFORM_NACL_MOJO_IRT_H_
