// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/geometry/geometry_type_converters.h"

namespace mojo {

// static
Point TypeConverter<Point, gfx::Point>::ConvertFrom(const gfx::Point& input,
                                                    Buffer* buf) {
  Point::Builder point(buf);
  point.set_x(input.x());
  point.set_y(input.y());
  return point.Finish();
}

// static
gfx::Point TypeConverter<Point, gfx::Point>::ConvertTo(const Point& input) {
  return gfx::Point(input.x(), input.y());
}

// static
Size TypeConverter<Size, gfx::Size>::ConvertFrom(const gfx::Size& input,
                                                 Buffer* buf) {
  Size::Builder size(buf);
  size.set_width(input.width());
  size.set_height(input.height());
  return size.Finish();
}

// static
gfx::Size TypeConverter<Size, gfx::Size>::ConvertTo(const Size& input) {
  return gfx::Size(input.width(), input.height());
}

// static
Rect TypeConverter<Rect, gfx::Rect>::ConvertFrom(const gfx::Rect& input,
                                                 Buffer* buf) {
  Rect::Builder rect(buf);
  rect.set_position(input.origin());
  rect.set_size(input.size());
  return rect.Finish();
}

// static
gfx::Rect TypeConverter<Rect, gfx::Rect>::ConvertTo(const Rect& input) {
  return gfx::Rect(input.position().x(), input.position().y(),
                    input.size().width(), input.size().height());
}

}  // namespace mojo
