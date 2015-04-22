// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_SWITCHES_H_
#define SHELL_SWITCHES_H_

#include <set>
#include <string>

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kChildProcess[];
extern const char kContentHandlers[];
extern const char kDisableCache[];
extern const char kDontDeleteOnDownload[];
extern const char kEnableMultiprocess[];
extern const char kForceInProcess[];
extern const char kHelp[];
extern const char kMapOrigin[];
extern const char kOrigin[];
extern const char kPredictableAppFilenames[];
extern const char kTraceStartup[];
extern const char kURLMappings[];

}  // namespace switches

#endif  // SHELL_SWITCHES_H_
