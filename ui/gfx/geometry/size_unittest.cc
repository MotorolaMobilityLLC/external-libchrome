// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/geometry/size_f.h"

namespace gfx {

namespace {

int TestSizeF(const SizeF& s) {
  return s.width();
}

}  // namespace

TEST(SizeTest, ToSizeF) {
  // Check that explicit conversion from integer to float compiles.
  Size a(10, 20);
  float width = TestSizeF(gfx::SizeF(a));
  EXPECT_EQ(width, a.width());

  SizeF b(10, 20);

  EXPECT_EQ(b, gfx::SizeF(a));
}

TEST(SizeTest, ToFlooredSize) {
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0, 0)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(0, 0), ToFlooredSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10, 10)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(10, 10), ToFlooredSize(SizeF(10.9999f, 10.9999f)));
}

TEST(SizeTest, ToCeiledSize) {
  EXPECT_EQ(Size(0, 0), ToCeiledSize(SizeF(0, 0)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(1, 1), ToCeiledSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToCeiledSize(SizeF(10, 10)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(11, 11), ToCeiledSize(SizeF(10.9999f, 10.9999f)));
}

TEST(SizeTest, ToRoundedSize) {
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0, 0)));
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0.0001f, 0.0001f)));
  EXPECT_EQ(Size(0, 0), ToRoundedSize(SizeF(0.4999f, 0.4999f)));
  EXPECT_EQ(Size(1, 1), ToRoundedSize(SizeF(0.5f, 0.5f)));
  EXPECT_EQ(Size(1, 1), ToRoundedSize(SizeF(0.9999f, 0.9999f)));

  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10, 10)));
  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10.0001f, 10.0001f)));
  EXPECT_EQ(Size(10, 10), ToRoundedSize(SizeF(10.4999f, 10.4999f)));
  EXPECT_EQ(Size(11, 11), ToRoundedSize(SizeF(10.5f, 10.5f)));
  EXPECT_EQ(Size(11, 11), ToRoundedSize(SizeF(10.9999f, 10.9999f)));
}

TEST(SizeTest, ClampSize) {
  Size a;

  a = Size(3, 5);
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
  a.SetToMax(Size(2, 4));
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
  a.SetToMax(Size(3, 5));
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
  a.SetToMax(Size(4, 2));
  EXPECT_EQ(Size(4, 5).ToString(), a.ToString());
  a.SetToMax(Size(8, 10));
  EXPECT_EQ(Size(8, 10).ToString(), a.ToString());

  a.SetToMin(Size(9, 11));
  EXPECT_EQ(Size(8, 10).ToString(), a.ToString());
  a.SetToMin(Size(8, 10));
  EXPECT_EQ(Size(8, 10).ToString(), a.ToString());
  a.SetToMin(Size(11, 9));
  EXPECT_EQ(Size(8, 9).ToString(), a.ToString());
  a.SetToMin(Size(7, 11));
  EXPECT_EQ(Size(7, 9).ToString(), a.ToString());
  a.SetToMin(Size(3, 5));
  EXPECT_EQ(Size(3, 5).ToString(), a.ToString());
}

TEST(SizeTest, ClampSizeF) {
  SizeF a;

  a = SizeF(3.5f, 5.5f);
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(2.5f, 4.5f));
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(3.5f, 5.5f));
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(4.5f, 2.5f));
  EXPECT_EQ(SizeF(4.5f, 5.5f).ToString(), a.ToString());
  a.SetToMax(SizeF(8.5f, 10.5f));
  EXPECT_EQ(SizeF(8.5f, 10.5f).ToString(), a.ToString());

  a.SetToMin(SizeF(9.5f, 11.5f));
  EXPECT_EQ(SizeF(8.5f, 10.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(8.5f, 10.5f));
  EXPECT_EQ(SizeF(8.5f, 10.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(11.5f, 9.5f));
  EXPECT_EQ(SizeF(8.5f, 9.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(7.5f, 11.5f));
  EXPECT_EQ(SizeF(7.5f, 9.5f).ToString(), a.ToString());
  a.SetToMin(SizeF(3.5f, 5.5f));
  EXPECT_EQ(SizeF(3.5f, 5.5f).ToString(), a.ToString());
}

TEST(SizeTest, Enlarge) {
  Size test(3, 4);
  test.Enlarge(5, -8);
  EXPECT_EQ(test, Size(8, -4));
}

TEST(SizeTest, IntegerOverflow) {
  int int_max = std::numeric_limits<int>::max();
  int int_min = std::numeric_limits<int>::min();

  Size max_size(int_max, int_max);
  Size min_size(int_min, int_min);
  Size test;

  test = Size();
  test.Enlarge(int_max, int_max);
  EXPECT_EQ(test, max_size);

  test = Size();
  test.Enlarge(int_min, int_min);
  EXPECT_EQ(test, min_size);

  test = Size(10, 20);
  test.Enlarge(int_max, int_max);
  EXPECT_EQ(test, max_size);

  test = Size(-10, -20);
  test.Enlarge(int_min, int_min);
  EXPECT_EQ(test, min_size);
}

}  // namespace gfx
