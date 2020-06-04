// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_EXPORT_H_
#define MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_EXPORT_H_

#if defined(WIN32)

#if defined(MOJO_VIEW_MANAGER_IMPLEMENTATION)
#define MOJO_VIEW_MANAGER_EXPORT __declspec(dllexport)
#else
#define MOJO_VIEW_MANAGER_EXPORT __declspec(dllimport)
#endif

#else  // !defined(WIN32)

#if defined(MOJO_VIEW_MANAGER_IMPLEMENTATION)
#define MOJO_VIEW_MANAGER_EXPORT __attribute__((visibility("default")))
#else
#define MOJO_VIEW_MANAGER_EXPORT
#endif

#endif  // defined(WIN32)

#endif  // MOJO_SERVICES_VIEW_MANAGER_VIEW_MANAGER_EXPORT_H_
