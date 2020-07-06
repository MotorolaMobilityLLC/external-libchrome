// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/native_handle_type_converters.h"

namespace mojo {

// static
IPC::MessageAttachment::Type
TypeConverter<IPC::MessageAttachment::Type, native::SerializedHandle_Type>::
    Convert(native::SerializedHandle_Type type) {
  switch (type) {
    case native::SerializedHandle_Type::MOJO_HANDLE:
      return IPC::MessageAttachment::Type::MOJO_HANDLE;
    case native::SerializedHandle_Type::PLATFORM_FILE:
      return IPC::MessageAttachment::Type::PLATFORM_FILE;
    case native::SerializedHandle_Type::WIN_HANDLE:
      return IPC::MessageAttachment::Type::WIN_HANDLE;
    case native::SerializedHandle_Type::MACH_PORT:
      return IPC::MessageAttachment::Type::MACH_PORT;
    case native::SerializedHandle_Type::FUCHSIA_HANDLE:
      return IPC::MessageAttachment::Type::FUCHSIA_HANDLE;
  }
  NOTREACHED();
  return IPC::MessageAttachment::Type::MOJO_HANDLE;
}

// static
native::SerializedHandle_Type TypeConverter<
    native::SerializedHandle_Type,
    IPC::MessageAttachment::Type>::Convert(IPC::MessageAttachment::Type type) {
  switch (type) {
    case IPC::MessageAttachment::Type::MOJO_HANDLE:
      return native::SerializedHandle_Type::MOJO_HANDLE;
    case IPC::MessageAttachment::Type::PLATFORM_FILE:
      return native::SerializedHandle_Type::PLATFORM_FILE;
    case IPC::MessageAttachment::Type::WIN_HANDLE:
      return native::SerializedHandle_Type::WIN_HANDLE;
    case IPC::MessageAttachment::Type::MACH_PORT:
      return native::SerializedHandle_Type::MACH_PORT;
    case IPC::MessageAttachment::Type::FUCHSIA_HANDLE:
      return native::SerializedHandle_Type::FUCHSIA_HANDLE;
  }
  NOTREACHED();
  return native::SerializedHandle_Type::MOJO_HANDLE;
}

}  // namespace mojo
