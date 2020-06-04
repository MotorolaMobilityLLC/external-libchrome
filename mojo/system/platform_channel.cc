// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/platform_channel.h"

#include "base/logging.h"

namespace mojo {
namespace system {

PlatformChannel::~PlatformChannel() {
  // Implementations must close the handle if necessary (e.g., if no one else
  // has taken ownership).
  DCHECK(!is_valid());
}

PlatformChannelHandle PlatformChannel::PassHandle() {
  DCHECK(is_valid());
  PlatformChannelHandle rv = handle_;
  handle_ = PlatformChannelHandle();
  return rv;
}

PlatformChannel::PlatformChannel() {
}

PlatformServerChannel::PlatformServerChannel(const std::string& name)
    : name_(name) {
  DCHECK(!name_.empty());
}

// Static factory method.
// static
scoped_ptr<PlatformClientChannel> PlatformClientChannel::CreateFromHandle(
      const PlatformChannelHandle& handle) {
  scoped_ptr<PlatformClientChannel> rv(new PlatformClientChannel());
  *rv->mutable_handle() = handle;
  return rv.Pass();
}

}  // namespace system
}  // namespace mojo
