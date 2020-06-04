// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tests/simple_bindings_support.h"
#include "mojom/test_structs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

struct RedmondRect {
  int32_t left;
  int32_t top;
  int32_t right;
  int32_t bottom;
};

struct RedmondNamedRegion {
  std::string name;
  std::vector<RedmondRect> rects;
};

}  // namespace

template <>
class SimilarityTraits<test_structs::Rect, RedmondRect> {
 public:
  static test_structs::Rect CopyFrom(const RedmondRect& input, Buffer* buf) {
    test_structs::Rect::Builder rect(buf);
    rect.set_x(input.left);
    rect.set_y(input.top);
    rect.set_width(input.right - input.left);
    rect.set_height(input.bottom - input.top);
    return rect.Finish();
  }
  static RedmondRect CopyTo(const test_structs::Rect& input) {
    RedmondRect rect;
    rect.left = input.x();
    rect.top = input.y();
    rect.right = input.x() + input.width();
    rect.bottom = input.y() + input.height();
    return rect;
  }
};

template <>
class SimilarityTraits<test_structs::NamedRegion, RedmondNamedRegion> {
 public:
  static test_structs::NamedRegion CopyFrom(const RedmondNamedRegion& input,
                                            Buffer* buf) {
    test_structs::NamedRegion::Builder region(buf);
    region.set_name(String(input.name, buf));
    region.set_rects(mojo::Array<test_structs::Rect>(input.rects, buf));
    return region.Finish();
  }
  static RedmondNamedRegion CopyTo(const test_structs::NamedRegion& input) {
    RedmondNamedRegion region;
    region.name = input.name().To<std::string>();
    region.rects = input.rects().To<std::vector<RedmondRect> >();
    return region;
  }
};

namespace test {

class BindingsTypeConversionTest : public testing::Test {
 private:
  SimpleBindingsSupport bindings_support_;
};

TEST_F(BindingsTypeConversionTest, String) {
  AllocationScope scope;

  const char kText[6] = "hello";

  String a = std::string(kText);
  String b(kText);
  String c(static_cast<const char*>(kText));

  EXPECT_EQ(std::string(kText), a.To<std::string>());
  EXPECT_EQ(std::string(kText), b.To<std::string>());
  EXPECT_EQ(std::string(kText), c.To<std::string>());
}

TEST_F(BindingsTypeConversionTest, String_ShallowCopy) {
  AllocationScope scope;

  String a("hello");
  String b(a);

  EXPECT_EQ(&a[0], &b[0]);
  EXPECT_EQ(a.To<std::string>(), b.To<std::string>());
}

TEST_F(BindingsTypeConversionTest, StringWithEmbeddedNull) {
  AllocationScope scope;

  const std::string kText("hel\0lo", 6);

  String a(kText);
  EXPECT_EQ(kText, a.To<std::string>());

  // Expect truncation:
  String b(kText.c_str());
  EXPECT_EQ(std::string("hel"), b.To<std::string>());
}

TEST_F(BindingsTypeConversionTest, CustomSimilarityTraits) {
  AllocationScope scope;

  test_structs::Rect::Builder rect_builder;
  rect_builder.set_x(10);
  rect_builder.set_y(20);
  rect_builder.set_width(50);
  rect_builder.set_height(45);
  test_structs::Rect rect = rect_builder.Finish();

  RedmondRect rr = rect.To<RedmondRect>();
  EXPECT_EQ(10, rr.left);
  EXPECT_EQ(20, rr.top);
  EXPECT_EQ(60, rr.right);
  EXPECT_EQ(65, rr.bottom);

  test_structs::Rect rect2(rr);
  EXPECT_EQ(rect.x(), rect2.x());
  EXPECT_EQ(rect.y(), rect2.y());
  EXPECT_EQ(rect.width(), rect2.width());
  EXPECT_EQ(rect.height(), rect2.height());
}

TEST_F(BindingsTypeConversionTest, CustomSimilarityTraits_Array) {
  AllocationScope scope;

  const RedmondRect kBase = { 10, 20, 30, 40 };

  Array<test_structs::Rect>::Builder rects_builder(10);
  for (size_t i = 0; i < rects_builder.size(); ++i) {
    RedmondRect rr = kBase;
    rr.left += static_cast<int32_t>(i);
    rr.top += static_cast<int32_t>(i);
    rects_builder[i] = test_structs::Rect(rr);
  }
  Array<test_structs::Rect> rects = rects_builder.Finish();

  std::vector<RedmondRect> redmond_rects =
      rects.To<std::vector<RedmondRect> >();

  // Note: assignment broken in two lines tests default constructor with
  // assignment operator. We will also test conversion constructor.
  Array<test_structs::Rect> rects2;
  rects2 = redmond_rects;

  EXPECT_EQ(rects.size(), rects2.size());
  for (size_t i = 0; i < rects.size(); ++i) {
    EXPECT_EQ(rects[i].x(), rects2[i].x());
    EXPECT_EQ(rects[i].y(), rects2[i].y());
    EXPECT_EQ(rects[i].width(), rects2[i].width());
    EXPECT_EQ(rects[i].height(), rects2[i].height());
  }

  // Test conversion constructor.
  Array<test_structs::Rect> rects3(redmond_rects);

  EXPECT_EQ(rects.size(), rects3.size());
  for (size_t i = 0; i < rects.size(); ++i) {
    EXPECT_EQ(rects[i].x(), rects3[i].x());
    EXPECT_EQ(rects[i].y(), rects3[i].y());
    EXPECT_EQ(rects[i].width(), rects3[i].width());
    EXPECT_EQ(rects[i].height(), rects3[i].height());
  }
}

TEST_F(BindingsTypeConversionTest, CustomSimilarityTraits_Array2) {
  AllocationScope scope;

  RedmondNamedRegion redmond_region;
  redmond_region.name = "foopy";

  const RedmondRect kBase = { 10, 20, 30, 40 };

  for (size_t i = 0; i < 10; ++i) {
    RedmondRect rect = kBase;
    rect.left += static_cast<int32_t>(i);
    rect.top += static_cast<int32_t>(i);
    redmond_region.rects.push_back(rect);
  }

  // Round-trip through generated struct and SimilarityTraits.

  test_structs::NamedRegion copy = redmond_region;
  RedmondNamedRegion redmond_region2 = copy.To<RedmondNamedRegion>();

  EXPECT_EQ(redmond_region.name, redmond_region2.name);
  EXPECT_EQ(redmond_region.rects.size(), redmond_region2.rects.size());
  for (size_t i = 0; i < redmond_region.rects.size(); ++i) {
    EXPECT_EQ(redmond_region.rects[i].left, redmond_region2.rects[i].left);
    EXPECT_EQ(redmond_region.rects[i].top, redmond_region2.rects[i].top);
    EXPECT_EQ(redmond_region.rects[i].right, redmond_region2.rects[i].right);
    EXPECT_EQ(redmond_region.rects[i].bottom, redmond_region2.rects[i].bottom);
  }
}

}  // namespace test
}  // namespace mojo
