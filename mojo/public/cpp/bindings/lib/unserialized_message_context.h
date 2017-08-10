// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_LIB_UNSERIALIZED_MESSAGE_CONTEXT_H_
#define MOJO_PUBLIC_CPP_BINDINGS_LIB_UNSERIALIZED_MESSAGE_CONTEXT_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/optional.h"
#include "mojo/public/c/system/types.h"
#include "mojo/public/cpp/bindings/bindings_export.h"
#include "mojo/public/cpp/bindings/lib/buffer.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_context.h"

namespace mojo {
namespace internal {

class MOJO_CPP_BINDINGS_EXPORT UnserializedMessageContext {
 public:
  struct Tag {};

  UnserializedMessageContext(const Tag* tag,
                             uint32_t message_name,
                             uint32_t message_flags);
  virtual ~UnserializedMessageContext();

  template <typename MessageType>
  MessageType* SafeCast() {
    if (&MessageType::kMessageTag != tag_)
      return nullptr;
    return static_cast<MessageType*>(this);
  }

  const Tag* tag() const { return tag_; }
  uint32_t message_name() const { return header_.name; }
  uint32_t message_flags() const { return header_.flags; }

  MessageHeaderV1* header() { return &header_; }

  virtual void Serialize(SerializationContext* serialization_context,
                         Buffer* buffer) = 0;

 private:
  // The |tag_| is used for run-time type identification of specific
  // unserialized message types, e.g. messages generated by mojom bindings. This
  // allows opaque message objects to be safely downcast once pulled off a pipe.
  const Tag* const tag_;

  // We store message metadata in a serialized header structure to simplify
  // Message implementation which needs to query such metadata for both
  // serialized and unserialized message objects.
  MessageHeaderV1 header_;

  DISALLOW_COPY_AND_ASSIGN(UnserializedMessageContext);
};

}  // namespace internal
}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_LIB_UNSERIALIZED_MESSAGE_CONTEXT_H_
