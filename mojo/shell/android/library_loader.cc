// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "mojo/shell/android/mojo_main.h"

namespace {

base::android::RegistrationMethod kMojoRegisteredMethods[] = {
  { "MojoMain", mojo::RegisterMojoMain },
};

bool RegisterJni(JNIEnv* env) {
  return RegisterNativeMethods(env, kMojoRegisteredMethods,
                               arraysize(kMojoRegisteredMethods));
}

}  // namespace

// This is called by the VM when the shared library is first loaded.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  base::android::InitVM(vm);
  JNIEnv* env = base::android::AttachCurrentThread();

  if (!RegisterJni(env))
    return -1;

  return JNI_VERSION_1_4;
}
