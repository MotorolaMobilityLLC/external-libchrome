// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/test/histogram_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class HistogramRecorderTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    HistogramRecorder::Initialize();
  }
};

TEST_F(HistogramRecorderTest, Scope) {
  // Record a histogram before the creation of the recorder.
  UMA_HISTOGRAM_BOOLEAN("Test", true);

  HistogramRecorder recorder;

  // Verify that no histogram is recorded.
  scoped_ptr<HistogramSamples> samples(
      recorder.GetHistogramSamplesSinceCreation("Test"));
  EXPECT_TRUE(samples);
  EXPECT_EQ(0, samples->TotalCount());

  // Record a histogram after the creation of the recorder.
  UMA_HISTOGRAM_BOOLEAN("Test", true);

  // Verify that one histogram is recorded.
  samples = recorder.GetHistogramSamplesSinceCreation("Test");
  EXPECT_TRUE(samples);
  EXPECT_EQ(1, samples->TotalCount());
}

}  // namespace base
