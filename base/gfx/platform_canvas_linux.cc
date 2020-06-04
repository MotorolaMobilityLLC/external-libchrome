// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/platform_canvas_linux.h"

#include "base/gfx/platform_device_linux.h"
#include "base/gfx/bitmap_platform_device_linux.h"
#include "base/logging.h"

namespace gfx {

PlatformCanvasLinux::PlatformCanvasLinux() : SkCanvas() {
}

PlatformCanvasLinux::PlatformCanvasLinux(int width, int height, bool is_opaque)
    : SkCanvas() {
  if (!initialize(width, height, is_opaque))
    CHECK(false);
}

PlatformCanvasLinux::~PlatformCanvasLinux() {
}

bool PlatformCanvasLinux::initialize(int width, int height, bool is_opaque) {
  SkDevice* device = createPlatformDevice(width, height, is_opaque);
  if (!device)
    return false;

  setDevice(device);
  device->unref(); // was created with refcount 1, and setDevice also refs
  return true;
}

PlatformDeviceLinux& PlatformCanvasLinux::getTopPlatformDevice() const {
  // All of our devices should be our special PlatformDevice.
  SkCanvas::LayerIter iter(const_cast<PlatformCanvasLinux*>(this), false);
  return *static_cast<PlatformDeviceLinux*>(iter.device());
}

SkDevice* PlatformCanvasLinux::createDevice(SkBitmap::Config, int width,
                                            int height, bool is_opaque) {
  return createPlatformDevice(width, height, is_opaque);
}

SkDevice* PlatformCanvasLinux::createPlatformDevice(int width,
                                                    int height,
                                                    bool is_opaque) {
  return BitmapPlatformDeviceLinux::Create(width, height, is_opaque);
}

}  // namespace gfx
