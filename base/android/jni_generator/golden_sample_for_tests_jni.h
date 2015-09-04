// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is autogenerated by
//     base/android/jni_generator/jni_generator.py
// For
//     org/chromium/example/jni_generator/SampleForTests

#ifndef org_chromium_example_jni_generator_SampleForTests_JNI
#define org_chromium_example_jni_generator_SampleForTests_JNI

#include <jni.h>

#include "base/android/jni_generator/jni_generator_helper.h"

#include "base/android/jni_int_wrapper.h"

// Step 1: forward declarations.
namespace {
const char kInnerStructAClassPath[] =
    "org/chromium/example/jni_generator/SampleForTests$InnerStructA";
const char kSampleForTestsClassPath[] =
    "org/chromium/example/jni_generator/SampleForTests";
const char kInnerStructBClassPath[] =
    "org/chromium/example/jni_generator/SampleForTests$InnerStructB";
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_InnerStructA_clazz = NULL;
#define InnerStructA_clazz(env) g_InnerStructA_clazz
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_SampleForTests_clazz = NULL;
#define SampleForTests_clazz(env) g_SampleForTests_clazz
// Leaking this jclass as we cannot use LazyInstance from some threads.
jclass g_InnerStructB_clazz = NULL;
#define InnerStructB_clazz(env) g_InnerStructB_clazz

}  // namespace

namespace base {
namespace android {

// Step 2: method stubs.

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& param);

static jlong
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeInit(JNIEnv*
    env, jobject jcaller,
    jstring param) {
  return Init(env, JavaParamRef<jobject>(env, jcaller),
      JavaParamRef<jstring>(env, param));
}

static void
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeDestroy(JNIEnv*
    env,
    jobject jcaller,
    jlong nativeCPPClass) {
  CPPClass* native = reinterpret_cast<CPPClass*>(nativeCPPClass);
  CHECK_NATIVE_PTR(env, jcaller, native, "Destroy");
  return native->Destroy(env, JavaParamRef<jobject>(env, jcaller));
}

static jdouble GetDoubleFunction(JNIEnv* env, const JavaParamRef<jobject>&
    jcaller);

static jdouble
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeGetDoubleFunction(JNIEnv*
    env, jobject jcaller) {
  return GetDoubleFunction(env, JavaParamRef<jobject>(env, jcaller));
}

static jfloat GetFloatFunction(JNIEnv* env, const JavaParamRef<jclass>&
    jcaller);

static jfloat
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeGetFloatFunction(JNIEnv*
    env, jclass jcaller) {
  return GetFloatFunction(env, JavaParamRef<jclass>(env, jcaller));
}

static void SetNonPODDatatype(JNIEnv* env, const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jobject>& rect);

static void
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeSetNonPODDatatype(JNIEnv*
    env, jobject jcaller,
    jobject rect) {
  return SetNonPODDatatype(env, JavaParamRef<jobject>(env, jcaller),
      JavaParamRef<jobject>(env, rect));
}

static ScopedJavaLocalRef<jobject> GetNonPODDatatype(JNIEnv* env, const
    JavaParamRef<jobject>& jcaller);

static jobject
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeGetNonPODDatatype(JNIEnv*
    env, jobject jcaller) {
  return GetNonPODDatatype(env, JavaParamRef<jobject>(env, jcaller)).Release();
}

static jint
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeMethod(JNIEnv*
    env,
    jobject jcaller,
    jlong nativeCPPClass) {
  CPPClass* native = reinterpret_cast<CPPClass*>(nativeCPPClass);
  CHECK_NATIVE_PTR(env, jcaller, native, "Method", 0);
  return native->Method(env, JavaParamRef<jobject>(env, jcaller));
}

static jdouble
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeMethodOtherP0(JNIEnv*
    env,
    jobject jcaller,
    jlong nativePtr) {
  CPPClass::InnerClass* native =
      reinterpret_cast<CPPClass::InnerClass*>(nativePtr);
  CHECK_NATIVE_PTR(env, jcaller, native, "MethodOtherP0", 0);
  return native->MethodOtherP0(env, JavaParamRef<jobject>(env, jcaller));
}

static void
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeAddStructB(JNIEnv*
    env,
    jobject jcaller,
    jlong nativeCPPClass,
    jobject b) {
  CPPClass* native = reinterpret_cast<CPPClass*>(nativeCPPClass);
  CHECK_NATIVE_PTR(env, jcaller, native, "AddStructB");
  return native->AddStructB(env, JavaParamRef<jobject>(env, jcaller),
      JavaParamRef<jobject>(env, b));
}

static void
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeIterateAndDoSomethingWithStructB(JNIEnv*
    env,
    jobject jcaller,
    jlong nativeCPPClass) {
  CPPClass* native = reinterpret_cast<CPPClass*>(nativeCPPClass);
  CHECK_NATIVE_PTR(env, jcaller, native, "IterateAndDoSomethingWithStructB");
  return native->IterateAndDoSomethingWithStructB(env,
      JavaParamRef<jobject>(env, jcaller));
}

static jstring
    Java_org_chromium_example_jni_1generator_SampleForTests_nativeReturnAString(JNIEnv*
    env,
    jobject jcaller,
    jlong nativeCPPClass) {
  CPPClass* native = reinterpret_cast<CPPClass*>(nativeCPPClass);
  CHECK_NATIVE_PTR(env, jcaller, native, "ReturnAString", NULL);
  return native->ReturnAString(env, JavaParamRef<jobject>(env,
      jcaller)).Release();
}

static base::subtle::AtomicWord g_SampleForTests_javaMethod = 0;
static jint Java_SampleForTests_javaMethod(JNIEnv* env, jobject obj,
    JniIntWrapper foo,
    JniIntWrapper bar) {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, obj,
      SampleForTests_clazz(env), 0);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, SampleForTests_clazz(env),
      "javaMethod",

"("
"I"
"I"
")"
"I",
      &g_SampleForTests_javaMethod);

  jint ret =
      env->CallIntMethod(obj,
          method_id, as_jint(foo), as_jint(bar));
  jni_generator::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_SampleForTests_staticJavaMethod = 0;
static jboolean Java_SampleForTests_staticJavaMethod(JNIEnv* env) {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, SampleForTests_clazz(env),
      SampleForTests_clazz(env), false);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, SampleForTests_clazz(env),
      "staticJavaMethod",

"("
")"
"Z",
      &g_SampleForTests_staticJavaMethod);

  jboolean ret =
      env->CallStaticBooleanMethod(SampleForTests_clazz(env),
          method_id);
  jni_generator::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_SampleForTests_packagePrivateJavaMethod = 0;
static void Java_SampleForTests_packagePrivateJavaMethod(JNIEnv* env, jobject
    obj) {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, obj,
      SampleForTests_clazz(env));
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, SampleForTests_clazz(env),
      "packagePrivateJavaMethod",

"("
")"
"V",
      &g_SampleForTests_packagePrivateJavaMethod);

     env->CallVoidMethod(obj,
          method_id);
  jni_generator::CheckException(env);

}

static base::subtle::AtomicWord g_SampleForTests_methodThatThrowsException = 0;
static void Java_SampleForTests_methodThatThrowsException(JNIEnv* env, jobject
    obj) {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, obj,
      SampleForTests_clazz(env));
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, SampleForTests_clazz(env),
      "methodThatThrowsException",

"("
")"
"V",
      &g_SampleForTests_methodThatThrowsException);

     env->CallVoidMethod(obj,
          method_id);

}

static base::subtle::AtomicWord g_InnerStructA_create = 0;
static ScopedJavaLocalRef<jobject> Java_InnerStructA_create(JNIEnv* env, jlong
    l,
    JniIntWrapper i,
    jstring s) {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, InnerStructA_clazz(env),
      InnerStructA_clazz(env), NULL);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_STATIC>(
      env, InnerStructA_clazz(env),
      "create",

"("
"J"
"I"
"Ljava/lang/String;"
")"
"Lorg/chromium/example/jni_generator/SampleForTests$InnerStructA;",
      &g_InnerStructA_create);

  jobject ret =
      env->CallStaticObjectMethod(InnerStructA_clazz(env),
          method_id, l, as_jint(i), s);
  jni_generator::CheckException(env);
  return ScopedJavaLocalRef<jobject>(env, ret);
}

static base::subtle::AtomicWord g_SampleForTests_addStructA = 0;
static void Java_SampleForTests_addStructA(JNIEnv* env, jobject obj, jobject a)
    {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, obj,
      SampleForTests_clazz(env));
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, SampleForTests_clazz(env),
      "addStructA",

"("
"Lorg/chromium/example/jni_generator/SampleForTests$InnerStructA;"
")"
"V",
      &g_SampleForTests_addStructA);

     env->CallVoidMethod(obj,
          method_id, a);
  jni_generator::CheckException(env);

}

static base::subtle::AtomicWord g_SampleForTests_iterateAndDoSomething = 0;
static void Java_SampleForTests_iterateAndDoSomething(JNIEnv* env, jobject obj)
    {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, obj,
      SampleForTests_clazz(env));
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, SampleForTests_clazz(env),
      "iterateAndDoSomething",

"("
")"
"V",
      &g_SampleForTests_iterateAndDoSomething);

     env->CallVoidMethod(obj,
          method_id);
  jni_generator::CheckException(env);

}

static base::subtle::AtomicWord g_InnerStructB_getKey = 0;
static jlong Java_InnerStructB_getKey(JNIEnv* env, jobject obj) {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, obj,
      InnerStructB_clazz(env), 0);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, InnerStructB_clazz(env),
      "getKey",

"("
")"
"J",
      &g_InnerStructB_getKey);

  jlong ret =
      env->CallLongMethod(obj,
          method_id);
  jni_generator::CheckException(env);
  return ret;
}

static base::subtle::AtomicWord g_InnerStructB_getValue = 0;
static ScopedJavaLocalRef<jstring> Java_InnerStructB_getValue(JNIEnv* env,
    jobject obj) {
  /* Must call RegisterNativesImpl()  */
  CHECK_CLAZZ(env, obj,
      InnerStructB_clazz(env), NULL);
  jmethodID method_id =
      base::android::MethodID::LazyGet<
      base::android::MethodID::TYPE_INSTANCE>(
      env, InnerStructB_clazz(env),
      "getValue",

"("
")"
"Ljava/lang/String;",
      &g_InnerStructB_getValue);

  jstring ret =
      static_cast<jstring>(env->CallObjectMethod(obj,
          method_id));
  jni_generator::CheckException(env);
  return ScopedJavaLocalRef<jstring>(env, ret);
}

// Step 3: RegisterNatives.

static const JNINativeMethod kMethodsSampleForTests[] = {
    { "nativeInit",
"("
"Ljava/lang/String;"
")"
"J",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeInit)
    },
    { "nativeDestroy",
"("
"J"
")"
"V",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeDestroy)
    },
    { "nativeGetDoubleFunction",
"("
")"
"D",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeGetDoubleFunction)
    },
    { "nativeGetFloatFunction",
"("
")"
"F",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeGetFloatFunction)
    },
    { "nativeSetNonPODDatatype",
"("
"Landroid/graphics/Rect;"
")"
"V",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeSetNonPODDatatype)
    },
    { "nativeGetNonPODDatatype",
"("
")"
"Ljava/lang/Object;",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeGetNonPODDatatype)
    },
    { "nativeMethod",
"("
"J"
")"
"I",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeMethod)
    },
    { "nativeMethodOtherP0",
"("
"J"
")"
"D",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeMethodOtherP0)
    },
    { "nativeAddStructB",
"("
"J"
"Lorg/chromium/example/jni_generator/SampleForTests$InnerStructB;"
")"
"V",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeAddStructB)
    },
    { "nativeIterateAndDoSomethingWithStructB",
"("
"J"
")"
"V",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeIterateAndDoSomethingWithStructB)
    },
    { "nativeReturnAString",
"("
"J"
")"
"Ljava/lang/String;",
    reinterpret_cast<void*>(Java_org_chromium_example_jni_1generator_SampleForTests_nativeReturnAString)
    },
};

static bool RegisterNativesImpl(JNIEnv* env) {

  g_InnerStructA_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kInnerStructAClassPath).obj()));
  g_SampleForTests_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kSampleForTestsClassPath).obj()));
  g_InnerStructB_clazz = reinterpret_cast<jclass>(env->NewGlobalRef(
      base::android::GetClass(env, kInnerStructBClassPath).obj()));

  const int kMethodsSampleForTestsSize = arraysize(kMethodsSampleForTests);

  if (env->RegisterNatives(SampleForTests_clazz(env),
                           kMethodsSampleForTests,
                           kMethodsSampleForTestsSize) < 0) {
    jni_generator::HandleRegistrationError(
        env, SampleForTests_clazz(env), __FILE__);
    return false;
  }

  return true;
}

}  // namespace android
}  // namespace base

#endif  // org_chromium_example_jni_generator_SampleForTests_JNI
