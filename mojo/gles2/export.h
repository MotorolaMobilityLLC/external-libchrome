// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GLES2_EXPORT_H_
#define MOJO_GLES2_EXPORT_H_

#if defined(COMPONENT_BUILD)
#if defined(WIN32)

#if defined(MOJO_GLES2_IMPL_IMPLEMENTATION)
#define MOJO_GLES2_IMPL_EXPORT __declspec(dllexport)
#else
#define MOJO_GLES2_IMPL_EXPORT __declspec(dllimport)
#endif  // defined(MOJO_GLES2_IMPL_IMPLEMENTATION)

#else  // defined(WIN32)
#if defined(MOJO_GLES2_IMPL_IMPLEMENTATION)
#define MOJO_GLES2_IMPL_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_GLES2_IMPL_EXPORT
#endif
#endif

#else  // defined(COMPONENT_BUILD)
#define MOJO_GLES2_IMPL_EXPORT
#endif

#endif  // MOJO_GLES2_EXPORT_H_
