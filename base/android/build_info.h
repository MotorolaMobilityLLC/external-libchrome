// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ANDROID_BUILD_INFO_H_
#define BASE_ANDROID_BUILD_INFO_H_

#include <jni.h>

#include <string>

#include "base/memory/singleton.h"

namespace base {
namespace android {

// BuildInfo is a singleton class that stores android build and device
// information. It will be called from Android specific java code and gets used
// primarily in crash reporting.
class BuildInfo {
 public:

  ~BuildInfo() {}

  // Static factory method for getting the singleton BuildInfo instance.
  // Note that ownership is not conferred on the caller and the BuildInfo in
  // question isn't actually freed until shutdown. This is ok because there
  // should only be one instance of BuildInfo ever created.
  static BuildInfo* GetInstance();

  // Const char* is used instead of std::strings because these values must be
  // available even if the process is in a crash state. Sadly
  // std::string.c_str() doesn't guarantee that memory won't be allocated when
  // it is called.
  const char* device() const {
    return device_;
  }

  const char* model() const {
    return model_;
  }

  const char* brand() const {
    return brand_;
  }

  const char* android_build_id() const {
    return android_build_id_;
  }

  const char* android_build_fp() const {
    return android_build_fp_;
  }

  const char* package_version_code() const {
    return package_version_code_;
  }

  const char* package_version_name() const {
    return package_version_name_;
  }

 private:
  BuildInfo();

  friend struct DefaultSingletonTraits<BuildInfo>;

  // Const char* is used instead of std::strings because these values must be
  // available even if the process is in a crash state. Sadly
  // std::string.c_str() doesn't guarantee that memory won't be allocated when
  // it is called.
  char* device_;
  char* model_;
  char* brand_;
  char* android_build_id_;
  char* android_build_fp_;
  char* package_version_code_;
  char* package_version_name_;

  DISALLOW_COPY_AND_ASSIGN(BuildInfo);
};

bool RegisterBuildInfo(JNIEnv* env);

}  // namespace android
}  // namespace base

#endif  // BASE_ANDROID_BUILD_INFO_H_
