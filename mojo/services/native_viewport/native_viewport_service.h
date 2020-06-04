// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_
#define MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_

#include "base/memory/scoped_vector.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/shell/service.h"
#include "mojo/services/native_viewport/native_viewport_export.h"
#include "mojo/shell/context.h"

#if defined(OS_ANDROID)
MOJO_NATIVE_VIEWPORT_EXPORT mojo::ServiceFactoryBase*
    CreateNativeViewportService(mojo::shell::Context* context,
                                mojo::ScopedMessagePipeHandle shell_handle);
#endif

#endif  // MOJO_SERVICES_NATIVE_VIEWPORT_SERVICE_H_
