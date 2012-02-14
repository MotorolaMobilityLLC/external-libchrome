// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "dbus/object_path.h"

namespace dbus {

bool ObjectPath::operator<(const ObjectPath& that) const {
  return value_ < that.value_;
}

bool ObjectPath::operator==(const ObjectPath& that) const {
  return value_ == that.value_;
}

bool ObjectPath::operator!=(const ObjectPath& that) const {
  return value_ != that.value_;
}

} // namespace dbus
