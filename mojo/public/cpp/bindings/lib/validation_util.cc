// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/validation_util.h"

#include <stdint.h>

#include <limits>

#include "base/strings/stringprintf.h"
#include "mojo/public/cpp/bindings/lib/message_internal.h"
#include "mojo/public/cpp/bindings/lib/serialization_util.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/interfaces/bindings/interface_control_messages.mojom.h"

namespace mojo {
namespace internal {

void ReportNonNullableValidationError(ValidationContext* validation_context,
                                      ValidationError error,
                                      int field_index) {
  const char* null_or_invalid =
      error == VALIDATION_ERROR_UNEXPECTED_NULL_POINTER ? "null" : "invalid";

  std::string error_message =
      base::StringPrintf("%s field %d", null_or_invalid, field_index);
  ReportValidationError(validation_context, error, error_message.c_str());
}

bool ValidateStructHeaderAndClaimMemory(const void* data,
                                        ValidationContext* validation_context) {
  if (!IsAligned(data)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }
  if (!validation_context->IsValidRange(data, sizeof(StructHeader))) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  const StructHeader* header = static_cast<const StructHeader*>(data);

  if (header->num_bytes < sizeof(StructHeader)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_UNEXPECTED_STRUCT_HEADER);
    return false;
  }

  if (!validation_context->ClaimMemory(data, header->num_bytes)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

bool ValidateNonInlinedUnionHeaderAndClaimMemory(
    const void* data,
    ValidationContext* validation_context) {
  if (!IsAligned(data)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MISALIGNED_OBJECT);
    return false;
  }

  if (!validation_context->ClaimMemory(data, kUnionDataSize) ||
      *static_cast<const uint32_t*>(data) != kUnionDataSize) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_ILLEGAL_MEMORY_RANGE);
    return false;
  }

  return true;
}

bool ValidateMessageIsRequestWithoutResponse(
    const Message* message,
    ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagIsResponse) ||
      message->has_flag(Message::kFlagExpectsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsRequestExpectingResponse(
    const Message* message,
    ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagIsResponse) ||
      !message->has_flag(Message::kFlagExpectsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool ValidateMessageIsResponse(const Message* message,
                               ValidationContext* validation_context) {
  if (message->has_flag(Message::kFlagExpectsResponse) ||
      !message->has_flag(Message::kFlagIsResponse)) {
    ReportValidationError(validation_context,
                          VALIDATION_ERROR_MESSAGE_HEADER_INVALID_FLAGS);
    return false;
  }
  return true;
}

bool IsHandleOrInterfaceValid(const AssociatedInterface_Data& input) {
  return input.handle.is_valid();
}

bool IsHandleOrInterfaceValid(const AssociatedEndpointHandle_Data& input) {
  return input.is_valid();
}

bool IsHandleOrInterfaceValid(const Interface_Data& input) {
  return input.handle.is_valid();
}

bool IsHandleOrInterfaceValid(const Handle_Data& input) {
  return input.is_valid();
}

bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedInterface_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportNonNullableValidationError(
      validation_context, VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
      field_index);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const AssociatedEndpointHandle_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportNonNullableValidationError(
      validation_context, VALIDATION_ERROR_UNEXPECTED_INVALID_INTERFACE_ID,
      field_index);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const Interface_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportNonNullableValidationError(validation_context,
                                   VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                                   field_index);
  return false;
}

bool ValidateHandleOrInterfaceNonNullable(
    const Handle_Data& input,
    int field_index,
    ValidationContext* validation_context) {
  if (IsHandleOrInterfaceValid(input))
    return true;

  ReportNonNullableValidationError(validation_context,
                                   VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE,
                                   field_index);
  return false;
}

bool ValidateHandleOrInterface(const AssociatedInterface_Data& input,
                               ValidationContext* validation_context) {
  if (validation_context->ClaimAssociatedEndpointHandle(input.handle))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
  return false;
}

bool ValidateHandleOrInterface(const AssociatedEndpointHandle_Data& input,
                               ValidationContext* validation_context) {
  if (validation_context->ClaimAssociatedEndpointHandle(input))
    return true;

  ReportValidationError(validation_context,
                        VALIDATION_ERROR_ILLEGAL_INTERFACE_ID);
  return false;
}

bool ValidateHandleOrInterface(const Interface_Data& input,
                               ValidationContext* validation_context) {
  if (validation_context->ClaimHandle(input.handle))
    return true;

  ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_HANDLE);
  return false;
}

bool ValidateHandleOrInterface(const Handle_Data& input,
                               ValidationContext* validation_context) {
  if (validation_context->ClaimHandle(input))
    return true;

  ReportValidationError(validation_context, VALIDATION_ERROR_ILLEGAL_HANDLE);
  return false;
}

}  // namespace internal
}  // namespace mojo
