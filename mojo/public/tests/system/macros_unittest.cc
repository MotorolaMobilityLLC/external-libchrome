// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file consists of "positive" tests, i.e., those verifying that things
// work (without compile errors, or even warnings if warnings are treated as
// errors).
// TODO(vtl): Fix no-compile tests (which are all disabled; crbug.com/105388)
// and write some "negative" tests.

#include "mojo/public/c/system/macros.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

TEST(MacrosTest, AllowUnused) {
  // Test that no warning/error is issued even though |x| is unused.
  int x MOJO_ALLOW_UNUSED = 123;
}

int MustUseReturnedResult() MOJO_WARN_UNUSED_RESULT;
int MustUseReturnedResult() {
  return 456;
}

TEST(MacrosTest, WarnUnusedResult) {
  if (!MustUseReturnedResult())
    abort();
}

// Note: MSVS is very strict (and arguably buggy) about warnings for classes
// defined in a local scope, so define these globally.
struct TestOverrideBaseClass {
  virtual ~TestOverrideBaseClass() {}
  virtual void ToBeOverridden() {}
  virtual void AlsoToBeOverridden() = 0;
};

struct TestOverrideSubclass : public TestOverrideBaseClass {
  virtual ~TestOverrideSubclass() {}
  virtual void ToBeOverridden() MOJO_OVERRIDE {}
  virtual void AlsoToBeOverridden() MOJO_OVERRIDE {}
};

TEST(MacrosTest, Override) {
  TestOverrideSubclass x;
  x.ToBeOverridden();
  x.AlsoToBeOverridden();
}

// Note: MSVS is very strict (and arguably buggy) about warnings for classes
// defined in a local scope, so define these globally.
class TestDisallowCopyAndAssignClass {
 public:
  TestDisallowCopyAndAssignClass() {}
  explicit TestDisallowCopyAndAssignClass(int) {}
  void NoOp() {}

 private:
  MOJO_DISALLOW_COPY_AND_ASSIGN(TestDisallowCopyAndAssignClass);
};

TEST(MacrosTest, DisallowCopyAndAssign) {
  TestDisallowCopyAndAssignClass x;
  x.NoOp();
  TestDisallowCopyAndAssignClass y(789);
  y.NoOp();
}

// First test |MOJO_COMPILE_ASSERT()| in a global scope.
MOJO_COMPILE_ASSERT(sizeof(int64_t) == 2 * sizeof(int32_t),
                    bad_compile_assert_failure_in_global_scope);

TEST(MacrosTest, CompileAssert) {
  // Then in a local scope.
  MOJO_COMPILE_ASSERT(sizeof(int32_t) == 2 * sizeof(int16_t),
                      bad_compile_assert_failure);
}

// Test that |MOJO_ARRAYSIZE()| works in a |MOJO_COMPILE_ASSERT()|.
const int kGlobalArray[5] = { 1, 2, 3, 4, 5 };
MOJO_COMPILE_ASSERT(MOJO_ARRAYSIZE(kGlobalArray) == 5u,
                    mojo_array_size_failed_in_compile_assert);

TEST(MacrosTest, ArraySize) {
  double local_array[4] = { 6.7, 7.8, 8.9, 9.0 };
  EXPECT_EQ(4u, MOJO_ARRAYSIZE(local_array));
}

// Note: MSVS is very strict (and arguably buggy) about warnings for classes
// defined in a local scope, so define these globally.
class MoveOnlyInt {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(MoveOnlyInt, RValue)

 public:
  MoveOnlyInt() : is_set_(false), value_() {}
  explicit MoveOnlyInt(int value) : is_set_(true), value_(value) {}
  ~MoveOnlyInt() {}

  // Move-only constructor and operator=.
  MoveOnlyInt(RValue other) { *this = other; }
  MoveOnlyInt& operator=(RValue other) {
    if (other.object != this) {
      is_set_ = other.object->is_set_;
      value_ = other.object->value_;
      other.object->is_set_ = false;
    }
    return *this;
  }

  int value() const {
    assert(is_set());
    return value_;
  }
  bool is_set() const { return is_set_; }

 private:
  bool is_set_;
  int value_;
};

TEST(MacrosTest, MoveOnlyTypeForCpp03) {
  MoveOnlyInt x(123);
  EXPECT_TRUE(x.is_set());
  EXPECT_EQ(123, x.value());
  MoveOnlyInt y;
  EXPECT_FALSE(y.is_set());
  y = x.Pass();
  EXPECT_FALSE(x.is_set());
  EXPECT_TRUE(y.is_set());
  EXPECT_EQ(123, y.value());
  MoveOnlyInt z(y.Pass());
  EXPECT_FALSE(y.is_set());
  EXPECT_TRUE(z.is_set());
  EXPECT_EQ(123, z.value());
  z = z.Pass();
  EXPECT_TRUE(z.is_set());
  EXPECT_EQ(123, z.value());
}

}  // namespace
}  // namespace mojo
