// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/gfx/bitmap_platform_device_linux.h"

#include <cairo/cairo.h>

#include "base/logging.h"

namespace gfx {

// -----------------------------------------------------------------------------
// These objects are reference counted and own a Cairo surface. The surface is
// the backing store for a Skia bitmap and we reference count it so that we can
// copy BitmapPlatformDeviceLinux objects without having to copy all the image
// data.
// -----------------------------------------------------------------------------
class BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinuxData
    : public base::RefCounted<BitmapPlatformDeviceLinuxData> {
 public:
  explicit BitmapPlatformDeviceLinuxData(cairo_surface_t* surface)
      : surface_(surface) { }

  cairo_surface_t* surface() const { return surface_; }

 protected:
  cairo_surface_t *const surface_;

  friend class base::RefCounted<BitmapPlatformDeviceLinuxData>;
  ~BitmapPlatformDeviceLinuxData() {
    cairo_surface_destroy(surface_);
  }

  DISALLOW_EVIL_CONSTRUCTORS(BitmapPlatformDeviceLinuxData);
};

// We use this static factory function instead of the regular constructor so
// that we can create the pixel data before calling the constructor. This is
// required so that we can call the base class' constructor with the pixel
// data.
BitmapPlatformDeviceLinux* BitmapPlatformDeviceLinux::Create(
    int width, int height, bool is_opaque) {
  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
                                 width, height);

  SkBitmap bitmap;
  bitmap.setConfig(SkBitmap::kARGB_8888_Config, width, height,
                   cairo_image_surface_get_stride(surface));
  bitmap.setPixels(cairo_image_surface_get_data(surface));
  bitmap.setIsOpaque(is_opaque);

#ifndef NDEBUG
  if (is_opaque) {
    bitmap.eraseARGB(255, 0, 255, 128);  // bright bluish green
  }
#endif

  // The device object will take ownership of the graphics context.
  return new BitmapPlatformDeviceLinux
      (bitmap, new BitmapPlatformDeviceLinuxData(surface));
}

// The device will own the bitmap, which corresponds to also owning the pixel
// data. Therefore, we do not transfer ownership to the SkDevice's bitmap.
BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(
    const SkBitmap& bitmap,
    BitmapPlatformDeviceLinuxData* data)
    : PlatformDeviceLinux(bitmap),
      data_(data) {
}

BitmapPlatformDeviceLinux::BitmapPlatformDeviceLinux(
    const BitmapPlatformDeviceLinux& other)
    : PlatformDeviceLinux(const_cast<BitmapPlatformDeviceLinux&>(
                          other).accessBitmap(true)),
      data_(other.data_) {
}

BitmapPlatformDeviceLinux::~BitmapPlatformDeviceLinux() {
}

cairo_surface_t* BitmapPlatformDeviceLinux::surface() const {
  return data_->surface();
}

BitmapPlatformDeviceLinux& BitmapPlatformDeviceLinux::operator=(
    const BitmapPlatformDeviceLinux& other) {
  data_ = other.data_;
  return *this;
}

}  // namespace gfx
