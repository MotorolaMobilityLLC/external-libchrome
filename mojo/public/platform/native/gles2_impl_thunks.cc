// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/platform/native/gles2_impl_thunks.h"

#include <assert.h>

#include "mojo/public/platform/native/thunk_export.h"

extern "C" {
static MojoGLES2ImplThunks g_impl_thunks = {0};

#define VISIT_GL_CALL(Function, ReturnType, PARAMETERS, ARGUMENTS) \
  ReturnType gl##Function PARAMETERS {                             \
    assert(g_impl_thunks.Function);                                \
    return g_impl_thunks.Function ARGUMENTS;                       \
  }
#include "mojo/public/c/gles2/gles2_call_visitor_autogen.h"
#undef VISIT_GL_CALL

extern "C" THUNK_EXPORT size_t
MojoSetGLES2ImplThunks(const MojoGLES2ImplThunks* gles2_impl_thunks) {
  if (gles2_impl_thunks->size >= sizeof(g_impl_thunks))
    g_impl_thunks = *gles2_impl_thunks;
  return sizeof(g_impl_thunks);
}

}  // extern "C"
