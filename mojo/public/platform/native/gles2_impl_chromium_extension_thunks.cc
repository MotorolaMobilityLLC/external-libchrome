// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/platform/native/gles2_impl_chromium_extension_thunks.h"

#include <assert.h>

#include "mojo/public/platform/native/thunk_export.h"

extern "C" {
static MojoGLES2ImplChromiumExtensionThunks g_impl_chromium_extension_thunks = {0};

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType GL_APIENTRY gl##Function PARAMETERS {                    \
    assert(g_impl_chromium_extension_thunks.Function);             \
    return g_impl_chromium_extension_thunks.Function ARGUMENTS;    \
  }
#include "mojo/public/c/gles2/gles2_call_visitor_chromium_extension_autogen.h"
#undef VISIT_GL_CALL

extern "C" THUNK_EXPORT size_t MojoSetGLES2ImplChromiumExtensionThunks(
    const MojoGLES2ImplChromiumExtensionThunks*
        gles2_impl_chromium_extension_thunks) {
  if (gles2_impl_chromium_extension_thunks->size >=
      sizeof(g_impl_chromium_extension_thunks))
    g_impl_chromium_extension_thunks = *gles2_impl_chromium_extension_thunks;
  return sizeof(g_impl_chromium_extension_thunks);
}

}  // extern "C"
