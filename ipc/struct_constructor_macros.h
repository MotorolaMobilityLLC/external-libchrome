// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_STRUCT_CONSTRUCTOR_MACROS_H_
#define IPC_STRUCT_CONSTRUCTOR_MACROS_H_

// Null out all the macros that need nulling.
#include "ipc/ipc_message_null_macros.h"

// Set up so next include will generate constructors.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#define IPC_STRUCT_BEGIN(struct_name) struct_name::struct_name() : NoParams()
#define IPC_STRUCT_MEMBER(type, name) , name()
#define IPC_STRUCT_END() {}

#endif  // IPC_STRUCT_CONSTRUCTOR_MACROS_H_

