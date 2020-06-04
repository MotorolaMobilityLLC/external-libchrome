// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GFX_PLATFORM_CANVAS_LINUX_H_
#define BASE_GFX_PLATFORM_CANVAS_LINUX_H_

#include "base/gfx/platform_device_linux.h"
#include "base/basictypes.h"

namespace gfx {

// This class is a specialization of the regular SkCanvas that is designed to
// work with a gfx::PlatformDevice to manage platform-specific drawing. It
// allows using both Skia operations and platform-specific operations.
class PlatformCanvasLinux : public SkCanvas {
 public:
  // Set is_opaque if you are going to erase the bitmap and not use
  // tranparency: this will enable some optimizations.  The shared_section
  // parameter is passed to gfx::PlatformDevice::create.  See it for details.
  //
  // If you use the version with no arguments, you MUST call initialize()
  PlatformCanvasLinux();
  PlatformCanvasLinux(int width, int height, bool is_opaque);
  virtual ~PlatformCanvasLinux();

  // For two-part init, call if you use the no-argument constructor above
  bool initialize(int width, int height, bool is_opaque);

  // Returns the platform device pointer of the topmost rect with a non-empty
  // clip. Both the windows and mac versions have an equivalent of this method;
  // a Linux version is added for compatibility.
  PlatformDeviceLinux& getTopPlatformDevice() const;

 protected:
  // Creates a device store for use by the canvas. We override this so that
  // the device is always our own so we know that we can use GDI operations
  // on it. Simply calls into createPlatformDevice().
  virtual SkDevice* createDevice(SkBitmap::Config, int width, int height,
                                 bool is_opaque);

  // Creates a device store for use by the canvas. By default, it creates a
  // BitmapPlatformDevice object. Can be overridden to change the object type.
  virtual SkDevice* createPlatformDevice(int width, int height, bool is_opaque);

  DISALLOW_COPY_AND_ASSIGN(PlatformCanvasLinux);
};

}  // namespace gfx

#endif  // BASE_GFX_PLATFORM_CANVAS_MAC_H_
