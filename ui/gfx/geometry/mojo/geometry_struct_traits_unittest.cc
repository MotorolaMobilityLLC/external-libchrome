// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/mojo/geometry_traits_test_service.mojom.h"
#include "ui/gfx/geometry/point.h"

namespace gfx {

namespace {

class GeometryStructTraitsTest : public testing::Test,
                                 public mojom::GeometryTraitsTestService {
 public:
  GeometryStructTraitsTest() {}

 protected:
  mojom::GeometryTraitsTestServicePtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // GeometryTraitsTestService:
  void EchoPoint(const Point& p, const EchoPointCallback& callback) override {
    callback.Run(p);
  }

  void EchoPointF(const PointF& p,
                  const EchoPointFCallback& callback) override {
    callback.Run(p);
  }

  void EchoSize(const Size& s, const EchoSizeCallback& callback) override {
    callback.Run(s);
  }

  void EchoSizeF(const SizeF& s, const EchoSizeFCallback& callback) override {
    callback.Run(s);
  }

  void EchoRect(const Rect& r, const EchoRectCallback& callback) override {
    callback.Run(r);
  }

  void EchoRectF(const RectF& r, const EchoRectFCallback& callback) override {
    callback.Run(r);
  }

  void EchoInsets(const Insets& i,
                  const EchoInsetsCallback& callback) override {
    callback.Run(i);
  }

  void EchoInsetsF(const InsetsF& i,
                   const EchoInsetsFCallback& callback) override {
    callback.Run(i);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<GeometryTraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(GeometryStructTraitsTest);
};

}  // namespace

TEST_F(GeometryStructTraitsTest, Point) {
  const int32_t x = 1234;
  const int32_t y = -5678;
  gfx::Point input(x, y);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::Point output;
  proxy->EchoPoint(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST_F(GeometryStructTraitsTest, PointF) {
  const float x = 1234.5f;
  const float y = 6789.6f;
  gfx::PointF input(x, y);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::PointF output;
  proxy->EchoPointF(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
}

TEST_F(GeometryStructTraitsTest, Size) {
  const int32_t width = 1234;
  const int32_t height = 5678;
  gfx::Size input(width, height);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::Size output;
  proxy->EchoSize(input, &output);
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, SizeF) {
  const float width = 1234.5f;
  const float height = 6789.6f;
  gfx::SizeF input(width, height);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::SizeF output;
  proxy->EchoSizeF(input, &output);
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, Rect) {
  const int32_t x = 1234;
  const int32_t y = 5678;
  const int32_t width = 4321;
  const int32_t height = 8765;
  gfx::Rect input(x, y, width, height);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::Rect output;
  proxy->EchoRect(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, RectF) {
  const float x = 1234.1f;
  const float y = 5678.2f;
  const float width = 4321.3f;
  const float height = 8765.4f;
  gfx::RectF input(x, y, width, height);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::RectF output;
  proxy->EchoRectF(input, &output);
  EXPECT_EQ(x, output.x());
  EXPECT_EQ(y, output.y());
  EXPECT_EQ(width, output.width());
  EXPECT_EQ(height, output.height());
}

TEST_F(GeometryStructTraitsTest, Insets) {
  const int32_t top = 1234;
  const int32_t left = 5678;
  const int32_t bottom = 4321;
  const int32_t right = 8765;
  gfx::Insets input(top, left, bottom, right);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::Insets output;
  proxy->EchoInsets(input, &output);
  EXPECT_EQ(top, output.top());
  EXPECT_EQ(left, output.left());
  EXPECT_EQ(bottom, output.bottom());
  EXPECT_EQ(right, output.right());
}

TEST_F(GeometryStructTraitsTest, InsetsF) {
  const float top = 1234.1f;
  const float left = 5678.2f;
  const float bottom = 4321.3f;
  const float right = 8765.4f;
  gfx::InsetsF input(top, left, bottom, right);
  mojom::GeometryTraitsTestServicePtr proxy = GetTraitsTestProxy();
  gfx::InsetsF output;
  proxy->EchoInsetsF(input, &output);
  EXPECT_EQ(top, output.top());
  EXPECT_EQ(left, output.left());
  EXPECT_EQ(bottom, output.bottom());
  EXPECT_EQ(right, output.right());
}

}  // namespace gfx
