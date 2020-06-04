// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/array.h"
#include "mojo/public/tests/bindings/simple_bindings_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

// Tests that basic Array operations work.
TEST(ArrayTest, Basic) {
  SimpleBindingsSupport bindings_support;

  // 8 bytes for the array, with 8 bytes left over for elements.
  internal::FixedBuffer buf(8 + 8*sizeof(char));
  EXPECT_EQ(16u, buf.size());

  Array<char>::Builder builder(8);
  EXPECT_EQ(8u, builder.size());
  for (size_t i = 0; i < builder.size(); ++i) {
    char val = static_cast<char>(i*2);
    builder[i] = val;
    EXPECT_EQ(val, builder.at(i));
  }
  Array<char> array = builder.Finish();
  for (size_t i = 0; i < array.size(); ++i) {
    char val = static_cast<char>(i) * 2;
    EXPECT_EQ(val, array[i]);
  }
}

// Tests that basic Array<bool> operations work, and that it's packed into 1
// bit per element.
TEST(ArrayTest, Bool) {
  SimpleBindingsSupport bindings_support;

  // 8 bytes for the array header, with 8 bytes left over for elements.
  internal::FixedBuffer buf(8 + 3);
  EXPECT_EQ(16u, buf.size());

  // An array of 64 bools can fit into 8 bytes.
  Array<bool>::Builder builder(64);
  EXPECT_EQ(64u, builder.size());
  for (size_t i = 0; i < builder.size(); ++i) {
    bool val = i % 3 == 0;
    builder[i] = val;
    EXPECT_EQ(val, builder.at(i));
  }
  Array<bool> array = builder.Finish();
  for (size_t i = 0; i < array.size(); ++i) {
    bool val = i % 3 == 0;
    EXPECT_EQ(val, array[i]);
  }
}

// Tests that Array<Handle> supports transferring handles.
TEST(ArrayTest, Handle) {
  SimpleBindingsSupport bindings_support;

  AllocationScope scope;

  ScopedMessagePipeHandle pipe0, pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  Array<MessagePipeHandle>::Builder handles_builder(2);
  handles_builder[0] = pipe0.Pass();
  handles_builder[1].reset(pipe1.release());

  EXPECT_FALSE(pipe0.is_valid());
  EXPECT_FALSE(pipe1.is_valid());

  Array<MessagePipeHandle> handles = handles_builder.Finish();
  EXPECT_TRUE(handles[0].is_valid());
  EXPECT_TRUE(handles[1].is_valid());

  pipe0 = handles[0].Pass();
  EXPECT_TRUE(pipe0.is_valid());
  EXPECT_FALSE(handles[0].is_valid());
}

}  // namespace test
}  // namespace mojo
