// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/histogram_recorder.h"

#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"

namespace base {

// static
void HistogramRecorder::Initialize() {
  // Ensure that StatisticsRecorder is initialized.
  StatisticsRecorder::Initialize();
}

HistogramRecorder::HistogramRecorder() {
  // Record any histogram data that exists when the object is created so it can
  // be subtracted later.
  StatisticsRecorder::Histograms histograms;
  StatisticsRecorder::GetSnapshot("", &histograms);
  for (size_t i = 0; i < histograms.size(); i++) {
    original_samples_[histograms[i]->histogram_name()] =
        histograms[i]->SnapshotSamples().release();
  }
}

HistogramRecorder::~HistogramRecorder() {
  STLDeleteValues(&original_samples_);
}

// static
bool HistogramRecorder::IsActive() {
  return StatisticsRecorder::IsActive();
}

scoped_ptr<HistogramSamples>
    HistogramRecorder::GetHistogramSamplesSinceCreation(
        const std::string& histogram_name) {
  HistogramBase* histogram = StatisticsRecorder::FindHistogram(histogram_name);
  if (!histogram)
    return scoped_ptr<HistogramSamples>();
  scoped_ptr<HistogramSamples> named_samples(histogram->SnapshotSamples());
  HistogramSamples* named_original_samples = original_samples_[histogram_name];
  if (named_original_samples)
    named_samples->Subtract(*named_original_samples);
  return named_samples.Pass();
}

}  // namespace base
