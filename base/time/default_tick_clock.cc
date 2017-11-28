// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/default_tick_clock.h"

namespace base {

DefaultTickClock::~DefaultTickClock() {}

TimeTicks DefaultTickClock::NowTicks() {
  return TimeTicks::Now();
}

// static
DefaultTickClock* DefaultTickClock::GetInstance() {
  CR_DEFINE_STATIC_LOCAL(DefaultTickClock, instance, ());
  return &instance;
}

}  // namespace base
