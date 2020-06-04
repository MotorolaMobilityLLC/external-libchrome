// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_MESSAGE_H_
#define MOJO_PUBLIC_BINDINGS_MESSAGE_H_

#include <assert.h>

#include <vector>

#include "mojo/public/bindings/lib/message_internal.h"

namespace mojo {

// Message is a holder for the data and handles to be sent over a MessagePipe.
// Message owns its data and handles, but a consumer of Message is free to
// mutate the data and handles members. The message's data is comprised of a
// header followed by payload.
class Message {
 public:
  Message();
  ~Message();

  // These may only be called on a newly created Message object.
  void AllocUninitializedData(uint32_t num_bytes);
  void AdoptData(uint32_t num_bytes, internal::MessageData* data);

  // Swaps data and handles between this Message and another.
  void Swap(Message* other);

  uint32_t data_num_bytes() const { return data_num_bytes_; }

  // Access the raw bytes of the message.
  const uint8_t* data() const { return
    reinterpret_cast<const uint8_t*>(data_);
  }
  uint8_t* mutable_data() { return reinterpret_cast<uint8_t*>(data_); }

  // Access the header.
  const internal::MessageHeader* header() const { return &data_->header; }

  // Access the request_id field (if present).
  bool has_request_id() const { return data_->header.num_fields == 3; }
  uint64_t request_id() const {
    assert(has_request_id());
    return static_cast<internal::MessageHeaderWithRequestID*>(&data_->header)->
        request_id;
  }

  // Access the payload.
  const uint8_t* payload() const { return data_->payload; }
  uint8_t* mutable_payload() { return data_->payload; }

  // Access the handles.
  const std::vector<Handle>* handles() const { return &handles_; }
  std::vector<Handle>* mutable_handles() { return &handles_; }

 private:
  uint32_t data_num_bytes_;
  internal::MessageData* data_;  // Heap-allocated using malloc.
  std::vector<Handle> handles_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(Message);
};

class MessageReceiver {
 public:
  // The receiver may mutate the given message.  Returns true if the message
  // was accepted and false otherwise, indicating that the message was invalid
  // or malformed.
  virtual bool Accept(Message* message) = 0;

  // A variant on Accept that registers a receiver to handle the response
  // message generated from the given message. The responder's Accept method
  // will be called some time after AcceptWithResponder returns. The responder
  // will be unregistered once its Accept method has been called.
  virtual bool AcceptWithResponder(Message* message,
                                   MessageReceiver* responder) = 0;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_MESSAGE_H_
