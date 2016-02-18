// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_snapshot_manager.h"

#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/stl_util.h"

namespace base {

HistogramSnapshotManager::HistogramSnapshotManager(
    HistogramFlattener* histogram_flattener)
    : preparing_deltas_(false),
      histogram_flattener_(histogram_flattener) {
  DCHECK(histogram_flattener_);
}

HistogramSnapshotManager::~HistogramSnapshotManager() {
}

void HistogramSnapshotManager::StartDeltas() {
  // Ensure that start/finish calls do not get nested.
  DCHECK(!preparing_deltas_);
  preparing_deltas_ = true;

#ifdef DEBUG
  for (const auto& iter : known_histograms) {
    CHECK(!iter->second.histogram);
    CHECK(!iter->second.accumulated_samples);
    CHECK(!(iter->second.inconsistencies &
            HistogramBase::NEW_INCONSISTENCY_FOUND));
  }
#endif
}

void HistogramSnapshotManager::PrepareDelta(HistogramBase* histogram) {
  PrepareSamples(histogram, histogram->SnapshotDelta());
}

void HistogramSnapshotManager::PrepareAbsolute(const HistogramBase* histogram) {
  PrepareSamples(histogram, histogram->SnapshotSamples());
}

void HistogramSnapshotManager::FinishDeltas() {
  DCHECK(preparing_deltas_);

  // Iterate over all known histograms to see what should be recorded.
  for (auto& iter : known_histograms_) {
    SampleInfo* sample_info = &iter.second;

    // First, record any histograms in which corruption was detected.
    if (sample_info->inconsistencies & HistogramBase::NEW_INCONSISTENCY_FOUND) {
      sample_info->inconsistencies &= ~HistogramBase::NEW_INCONSISTENCY_FOUND;
      histogram_flattener_->UniqueInconsistencyDetected(
          static_cast<HistogramBase::Inconsistency>(
              sample_info->inconsistencies));
    }

    // Second, record actual accumulated deltas.
    if (sample_info->accumulated_samples) {
      // TODO(bcwhite): Investigate using redundant_count() below to avoid
      // additional pass through all the samples to calculate real total.
      if (sample_info->accumulated_samples->TotalCount() > 0) {
        histogram_flattener_->RecordDelta(*sample_info->histogram,
                                          *sample_info->accumulated_samples);
      }
      delete sample_info->accumulated_samples;
      sample_info->accumulated_samples = nullptr;
    }

    // The Histogram pointer must be cleared at this point because the owner
    // is only required to keep it alive until FinishDeltas() completes.
    sample_info->histogram = nullptr;
  }

  preparing_deltas_ = false;
}

void HistogramSnapshotManager::PrepareSamples(
    const HistogramBase* histogram,
    scoped_ptr<HistogramSamples> samples) {
  DCHECK(histogram_flattener_);

  // Get information known about this histogram.
  SampleInfo* sample_info = &known_histograms_[histogram->name_hash()];
  if (sample_info->histogram) {
    DCHECK_EQ(sample_info->histogram->histogram_name(),
              histogram->histogram_name()) << "hash collision";
  } else {
    // First time this histogram has been seen; datafill.
    sample_info->histogram = histogram;
  }

  // Crash if we detect that our histograms have been overwritten.  This may be
  // a fair distance from the memory smasher, but we hope to correlate these
  // crashes with other events, such as plugins, or usage patterns, etc.
  uint32_t corruption = histogram->FindCorruption(*samples);
  if (HistogramBase::BUCKET_ORDER_ERROR & corruption) {
    // The checksum should have caught this, so crash separately if it didn't.
    CHECK_NE(0U, HistogramBase::RANGE_CHECKSUM_ERROR & corruption);
    CHECK(false);  // Crash for the bucket order corruption.
  }
  // Checksum corruption might not have caused order corruption.
  CHECK_EQ(0U, HistogramBase::RANGE_CHECKSUM_ERROR & corruption);

  // Note, at this point corruption can only be COUNT_HIGH_ERROR or
  // COUNT_LOW_ERROR and they never arise together, so we don't need to extract
  // bits from corruption.
  if (corruption) {
    DLOG(ERROR) << "Histogram: \"" << histogram->histogram_name()
                << "\" has data corruption: " << corruption;
    histogram_flattener_->InconsistencyDetected(
        static_cast<HistogramBase::Inconsistency>(corruption));
    // Don't record corrupt data to metrics services.
    const uint32_t old_corruption = sample_info->inconsistencies;
    if (old_corruption == (corruption | old_corruption))
      return;  // We've already seen this corruption for this histogram.
    sample_info->inconsistencies |=
        corruption | HistogramBase::NEW_INCONSISTENCY_FOUND;
    // TODO(bcwhite): Can we clear the inconsistency for future collection?
    return;
  }

  if (!sample_info->accumulated_samples) {
    // This histogram has not been seen before; add it as a new entry.
    sample_info->accumulated_samples = samples.release();
  } else {
    // There are previous values from this histogram; add them together.
    sample_info->accumulated_samples->Add(*samples);
  }
}

void HistogramSnapshotManager::InspectLoggedSamplesInconsistency(
      const HistogramSamples& new_snapshot,
      HistogramSamples* logged_samples) {
  HistogramBase::Count discrepancy =
      logged_samples->TotalCount() - logged_samples->redundant_count();
  if (!discrepancy)
    return;

  histogram_flattener_->InconsistencyDetectedInLoggedCount(discrepancy);
  if (discrepancy > Histogram::kCommonRaceBasedCountMismatch) {
    // Fix logged_samples.
    logged_samples->Subtract(*logged_samples);
    logged_samples->Add(new_snapshot);
  }
}

}  // namespace base
