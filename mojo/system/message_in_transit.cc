// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/message_in_transit.h"

#include <string.h>

#include <new>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "mojo/system/constants.h"

namespace mojo {
namespace system {

STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeMessagePipeEndpoint;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeMessagePipe;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Type
    MessageInTransit::kTypeChannel;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeMessagePipeEndpointData;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeChannelRunMessagePipeEndpoint;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpoint;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::Subtype
    MessageInTransit::kSubtypeChannelRemoveMessagePipeEndpointAck;
STATIC_CONST_MEMBER_DEFINITION const MessageInTransit::EndpointId
    MessageInTransit::kInvalidEndpointId;
STATIC_CONST_MEMBER_DEFINITION const size_t MessageInTransit::kMessageAlignment;
STATIC_CONST_MEMBER_DEFINITION const size_t
    MessageInTransit::kMaxSerializedDispatcherSize;
STATIC_CONST_MEMBER_DEFINITION const size_t
    MessageInTransit::kMaxSerializedDispatcherPlatformHandles;

struct MessageInTransit::PrivateStructForCompileAsserts {
  // The size of |Header| must be a multiple of the alignment.
  COMPILE_ASSERT(sizeof(Header) % kMessageAlignment == 0,
                 sizeof_MessageInTransit_Header_invalid);
  // Avoid dangerous situations, but making sure that the size of the "header" +
  // the size of the data fits into a 31-bit number.
  COMPILE_ASSERT(static_cast<uint64_t>(sizeof(Header)) + kMaxMessageNumBytes <=
                     0x7fffffffULL,
                 kMaxMessageNumBytes_too_big);

  // We assume (to avoid extra rounding code) that the maximum message (data)
  // size is a multiple of the alignment.
  COMPILE_ASSERT(kMaxMessageNumBytes % kMessageAlignment == 0,
                 kMessageAlignment_not_a_multiple_of_alignment);

  // The maximum serialized dispatcher size must be a multiple of the alignment.
  COMPILE_ASSERT(kMaxSerializedDispatcherSize % kMessageAlignment == 0,
                 kMaxSerializedDispatcherSize_not_a_multiple_of_alignment);

  // The size of |HandleTableEntry| must be a multiple of the alignment.
  COMPILE_ASSERT(sizeof(HandleTableEntry) % kMessageAlignment == 0,
                 sizeof_MessageInTransit_HandleTableEntry_invalid);
};

// For each attached (Mojo) handle, there'll be a handle table entry and
// serialized dispatcher data.
// static
const size_t MessageInTransit::kMaxSecondaryBufferSize = kMaxMessageNumHandles *
    (sizeof(HandleTableEntry) + kMaxSerializedDispatcherSize);

// static
const size_t MessageInTransit::kMaxPlatformHandles =
    kMaxMessageNumHandles * kMaxSerializedDispatcherPlatformHandles;

MessageInTransit::View::View(size_t message_size, const void* buffer)
    : buffer_(buffer) {
  size_t next_message_size = 0;
  DCHECK(MessageInTransit::GetNextMessageSize(buffer_, message_size,
                                              &next_message_size));
  DCHECK_EQ(message_size, next_message_size);
  // This should be equivalent.
  DCHECK_EQ(message_size, total_size());
}

bool MessageInTransit::View::IsValid(const char** error_message) const {
  // Note: This also implies a check on the |main_buffer_size()|, which is just
  // |RoundUpMessageAlignment(sizeof(Header) + num_bytes())|.
  if (num_bytes() > kMaxMessageNumBytes) {
    *error_message = "Message data payload too large";
    return false;
  }

  if (const char* secondary_buffer_error_message =
          ValidateSecondaryBuffer(num_handles(), secondary_buffer(),
                                  secondary_buffer_size())) {
    *error_message = secondary_buffer_error_message;
    return false;
  }

  return true;
}

MessageInTransit::MessageInTransit(Type type,
                                   Subtype subtype,
                                   uint32_t num_bytes,
                                   uint32_t num_handles,
                                   const void* bytes)
    : main_buffer_size_(RoundUpMessageAlignment(sizeof(Header) + num_bytes)),
      main_buffer_(static_cast<char*>(base::AlignedAlloc(main_buffer_size_,
                                                         kMessageAlignment))),
      secondary_buffer_size_(0) {
  DCHECK_LE(num_bytes, kMaxMessageNumBytes);
  DCHECK_LE(num_handles, kMaxMessageNumHandles);

  // |total_size| is updated below, from the other values.
  header()->type = type;
  header()->subtype = subtype;
  header()->source_id = kInvalidEndpointId;
  header()->destination_id = kInvalidEndpointId;
  header()->num_bytes = num_bytes;
  header()->num_handles = num_handles;
  // Note: If dispatchers are subsequently attached (in particular, if
  // |num_handles| is nonzero), then |total_size| will have to be adjusted.
  UpdateTotalSize();

  if (bytes) {
    memcpy(MessageInTransit::bytes(), bytes, num_bytes);
    memset(static_cast<char*>(MessageInTransit::bytes()) + num_bytes, 0,
           main_buffer_size_ - sizeof(Header) - num_bytes);
  } else {
    memset(MessageInTransit::bytes(), 0, main_buffer_size_ - sizeof(Header));
  }
}

// TODO(vtl): Do I really want/need to copy the secondary buffer here, or should
// I just create (deserialize) the dispatchers right away?
MessageInTransit::MessageInTransit(const View& message_view)
    : main_buffer_size_(message_view.main_buffer_size()),
      main_buffer_(static_cast<char*>(base::AlignedAlloc(main_buffer_size_,
                                                         kMessageAlignment))),
      secondary_buffer_size_(message_view.secondary_buffer_size()),
      secondary_buffer_(secondary_buffer_size_ ?
                            static_cast<char*>(
                                base::AlignedAlloc(secondary_buffer_size_,
                                                   kMessageAlignment)) : NULL) {
  DCHECK_GE(main_buffer_size_, sizeof(Header));
  DCHECK_EQ(main_buffer_size_ % kMessageAlignment, 0u);

  memcpy(main_buffer_.get(), message_view.main_buffer(), main_buffer_size_);
  memcpy(secondary_buffer_.get(), message_view.secondary_buffer(),
         secondary_buffer_size_);

  DCHECK_EQ(main_buffer_size_,
            RoundUpMessageAlignment(sizeof(Header) + num_bytes()));
}

MessageInTransit::~MessageInTransit() {
  if (dispatchers_) {
    for (size_t i = 0; i < dispatchers_->size(); i++) {
      if (!(*dispatchers_)[i])
        continue;

      DCHECK((*dispatchers_)[i]->HasOneRef());
      (*dispatchers_)[i]->Close();
    }
  }

  if (platform_handles_) {
    for (size_t i = 0; i < platform_handles_->size(); i++)
      (*platform_handles_)[i].CloseIfNecessary();
  }

#ifndef NDEBUG
  secondary_buffer_size_ = 0;
  dispatchers_.reset();
  platform_handles_.reset();
#endif
}

// static
bool MessageInTransit::GetNextMessageSize(const void* buffer,
                                          size_t buffer_size,
                                          size_t* next_message_size) {
  DCHECK(next_message_size);
  if (!buffer_size)
    return false;
  DCHECK(buffer);
  DCHECK_EQ(reinterpret_cast<uintptr_t>(buffer) %
                MessageInTransit::kMessageAlignment, 0u);

  if (buffer_size < sizeof(Header))
    return false;

  const Header* header = static_cast<const Header*>(buffer);
  *next_message_size = header->total_size;
  DCHECK_EQ(*next_message_size % kMessageAlignment, 0u);
  return true;
}

void MessageInTransit::SetDispatchers(
    scoped_ptr<std::vector<scoped_refptr<Dispatcher> > > dispatchers) {
  DCHECK(dispatchers);
  DCHECK(!dispatchers_);

  dispatchers_ = dispatchers.Pass();
#ifndef NDEBUG
  for (size_t i = 0; i < dispatchers_->size(); i++)
    DCHECK(!(*dispatchers_)[i] || (*dispatchers_)[i]->HasOneRef());
#endif
}

void MessageInTransit::SerializeAndCloseDispatchers(Channel* channel) {
  DCHECK(channel);
  DCHECK(!secondary_buffer_);

  const size_t num_handles = dispatchers_ ? dispatchers_->size() : 0;
  if (!num_handles)
    return;

  // The offset to the start of the (Mojo) handle table.
  // TODO(vtl): Add a header to the secondary buffer.
  const size_t handle_table_start_offset = 0;
  // The offset to the start of the serialized dispatcher data.
  const size_t serialized_dispatcher_start_offset =
      handle_table_start_offset + num_handles * sizeof(HandleTableEntry);
  // The estimated size of the secondary buffer. We compute this estimate below.
  // It must be at least as big as the (eventual) actual size.
  size_t estimated_size = serialized_dispatcher_start_offset;
  size_t num_platform_handles = 0;
#if DCHECK_IS_ON
  std::vector<size_t> all_max_sizes(num_handles);
  std::vector<size_t> all_max_platform_handles(num_handles);
#endif
  for (size_t i = 0; i < num_handles; i++) {
    if (Dispatcher* dispatcher = (*dispatchers_)[i].get()) {
      size_t max_size = 0;
      size_t max_platform_handles = 0;
      Dispatcher::MessageInTransitAccess::StartSerialize(
              dispatcher, channel, &max_size, &max_platform_handles);

      DCHECK_LE(max_size, kMaxSerializedDispatcherSize);
      estimated_size += RoundUpMessageAlignment(max_size);
      DCHECK_LE(estimated_size, kMaxSecondaryBufferSize);

      DCHECK_LE(max_platform_handles,
                kMaxSerializedDispatcherPlatformHandles);
      num_platform_handles += max_platform_handles;
      DCHECK_LE(num_platform_handles, kMaxPlatformHandles);

#if DCHECK_IS_ON
      all_max_sizes[i] = max_size;
      all_max_platform_handles[i] = max_platform_handles;
#endif
    }
  }

  secondary_buffer_.reset(static_cast<char*>(
      base::AlignedAlloc(estimated_size, kMessageAlignment)));
  // Entirely clear out the secondary buffer, since then we won't have to worry
  // about clearing padding or unused space (e.g., if a dispatcher fails to
  // serialize).
  memset(secondary_buffer_.get(), 0, estimated_size);

  if (num_platform_handles > 0) {
    DCHECK(!platform_handles_);
    platform_handles_.reset(new std::vector<embedder::PlatformHandle>());
  }

  HandleTableEntry* handle_table = reinterpret_cast<HandleTableEntry*>(
      secondary_buffer_.get() + handle_table_start_offset);
  size_t current_offset = serialized_dispatcher_start_offset;
  for (size_t i = 0; i < num_handles; i++) {
    Dispatcher* dispatcher = (*dispatchers_)[i].get();
    if (!dispatcher) {
      COMPILE_ASSERT(Dispatcher::kTypeUnknown == 0,
                     value_of_Dispatcher_kTypeUnknown_must_be_zero);
      continue;
    }

#if DCHECK_IS_ON
    size_t old_platform_handles_size =
        platform_handles_ ? platform_handles_->size() : 0;
#endif

    void* destination = secondary_buffer_.get() + current_offset;
    size_t actual_size = 0;
    if (Dispatcher::MessageInTransitAccess::EndSerializeAndClose(
            dispatcher, channel, destination, &actual_size,
            platform_handles_.get())) {
      handle_table[i].type = static_cast<int32_t>(dispatcher->GetType());
      handle_table[i].offset = static_cast<uint32_t>(current_offset);
      handle_table[i].size = static_cast<uint32_t>(actual_size);

#if DCHECK_IS_ON
      DCHECK_LE(actual_size, all_max_sizes[i]);
      DCHECK_LE(platform_handles_ ? (platform_handles_->size() -
                                         old_platform_handles_size) : 0,
                all_max_platform_handles[i]);
#endif
    } else {
      // Nothing to do on failure, since |secondary_buffer_| was cleared, and
      // |kTypeUnknown| is zero. The handle was simply closed.
      LOG(ERROR) << "Failed to serialize handle to remote message pipe";
    }

    current_offset += RoundUpMessageAlignment(actual_size);
    DCHECK_LE(current_offset, estimated_size);
    DCHECK_LE(platform_handles_ ? platform_handles_->size() : 0,
              num_platform_handles);
  }

  // There's no aligned realloc, so it's no good way to release unused space (if
  // we overshot our estimated space requirements).
  secondary_buffer_size_ = current_offset;

  // The dispatchers (which we "owned") were closed. We can dispose of them now.
  dispatchers_.reset();

  // Update the sizes in the message header.
  UpdateTotalSize();
}

// Note: The message's secondary buffer should have been checked by calling
// |View::IsValid()| (on the |View|) first.
void MessageInTransit::DeserializeDispatchers(Channel* channel) {
  DCHECK(!dispatchers_);

  // Already checked by |View::IsValid()|:
  DCHECK_LE(num_handles(), kMaxMessageNumHandles);

  if (!num_handles())
    return;

  dispatchers_.reset(
      new std::vector<scoped_refptr<Dispatcher> >(num_handles()));

  size_t handle_table_size = num_handles() * sizeof(HandleTableEntry);
  // Already checked by |View::IsValid()|:
  DCHECK_LE(handle_table_size, secondary_buffer_size_);

  const HandleTableEntry* handle_table =
      reinterpret_cast<const HandleTableEntry*>(secondary_buffer_.get());
  for (size_t i = 0; i < num_handles(); i++) {
    size_t offset = handle_table[i].offset;
    size_t size = handle_table[i].size;
    // Already checked by |View::IsValid()|:
    DCHECK_EQ(offset % kMessageAlignment, 0u);
    DCHECK_LE(offset, secondary_buffer_size_);
    DCHECK_LE(offset + size, secondary_buffer_size_);

    const void* source = secondary_buffer_.get() + offset;
    (*dispatchers_)[i] = Dispatcher::MessageInTransitAccess::Deserialize(
        channel, handle_table[i].type, source, size);
  }
}

// Validates the secondary buffer. Returns null on success, or a human-readable
// error message on error.
// static
const char* MessageInTransit::ValidateSecondaryBuffer(
    size_t num_handles,
    const void* secondary_buffer,
    size_t secondary_buffer_size) {
  // Always make sure that the secondary buffer size is sane (even if we have no
  // handles); if it's not, someone's messing with us.
  if (secondary_buffer_size > kMaxSecondaryBufferSize)
    return "Message secondary buffer too large";

  // Fast-path for the common case (no handles => no secondary buffer).
  if (num_handles == 0) {
    // We shouldn't have a secondary buffer in this case.
    if (secondary_buffer_size > 0)
      return "Message has no handles attached, but secondary buffer present";
    return NULL;
  }

  // Sanity-check |num_handles| (before multiplying it against anything).
  if (num_handles > kMaxMessageNumHandles)
    return "Message handle payload too large";

  if (secondary_buffer_size < num_handles * sizeof(HandleTableEntry))
    return "Message secondary buffer too small";

  DCHECK(secondary_buffer);
  const HandleTableEntry* handle_table =
      static_cast<const HandleTableEntry*>(secondary_buffer);

  static const char kInvalidSerializedDispatcher[] =
      "Message contains invalid serialized dispatcher";
  for (size_t i = 0; i < num_handles; i++) {
    size_t offset = handle_table[i].offset;
    if (offset % kMessageAlignment != 0)
      return kInvalidSerializedDispatcher;

    size_t size = handle_table[i].size;
    if (size > kMaxSerializedDispatcherSize || size > secondary_buffer_size)
      return kInvalidSerializedDispatcher;

    // Note: This is an overflow-safe check for |offset + size >
    // secondary_buffer_size()| (we know that |size <= secondary_buffer_size()|
    // from the previous check).
    if (offset > secondary_buffer_size - size)
      return kInvalidSerializedDispatcher;
  }

  return NULL;
}

void MessageInTransit::UpdateTotalSize() {
  DCHECK_EQ(main_buffer_size_ % kMessageAlignment, 0u);
  DCHECK_EQ(secondary_buffer_size_ % kMessageAlignment, 0u);
  header()->total_size =
      static_cast<uint32_t>(main_buffer_size_ + secondary_buffer_size_);
}

}  // namespace system
}  // namespace mojo
