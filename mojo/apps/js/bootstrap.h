// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_APPS_JS_BOOTSTRAP_H_
#define MOJO_APPS_JS_BOOTSTRAP_H_

#include "mojo/public/system/core.h"
#include "v8/include/v8.h"

namespace mojo {
namespace apps {

class Bootstrap {
 public:
  static const char kModuleName[];
  static v8::Local<v8::ObjectTemplate> GetTemplate(v8::Isolate* isolate);

  // Must be called before the first call to GetTemplate.
  static void SetInitialHandle(MojoHandle handle);
};

}  // namespace apps
}  // namespace mojo

#endif  // MOJO_APPS_JS_BOOTSTRAP_H_
