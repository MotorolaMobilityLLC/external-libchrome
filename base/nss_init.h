// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NSS_INIT_H_
#define BASE_NSS_INIT_H_

namespace base {

// Initialize NRPR if it isn't already initialized.  This function is
// thread-safe, and NSPR will only ever be initialized once.  NSPR will be
// properly shut down on program exit.
void EnsureNSPRInit();

// Initialize NSS if it isn't already initialized.  This must be called before
// any other NSS functions.  This function is thread-safe, and NSS will only
// ever be initialized once.  NSS will be properly shut down on program exit.
void EnsureNSSInit();

}  // namespace base

#endif  // BASE_NSS_INIT_H_
