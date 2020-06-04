// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_EVENT_TYPES_H
#define BASE_EVENT_TYPES_H
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#if defined(USE_X11)
typedef union _XEvent XEvent;
#endif

namespace base {

// Cross platform typedefs for native event types.
#if defined(OS_WIN)
typedef MSG NativeEvent;
#elif defined(USE_X11)
typedef XEvent* NativeEvent;
#elif defined(USE_WAYLAND)
// WaylandEvent is currently defined in ui component and cannot be
// used in base component. Probably wayland maintainer has to move it
// outside of ui in order to define NativeEvent for wayland here.
typedef void* NativeEvent;
#else
typedef void* NativeEvent;
#endif

} // namespace base

#endif  // BASE_EVENT_TYPES_H
