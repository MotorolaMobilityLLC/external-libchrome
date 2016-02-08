// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <climits>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/bucket_ranges.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_persistence.h"
#include "base/metrics/persistent_memory_allocator.h"
#include "base/metrics/sample_vector.h"
#include "base/metrics/statistics_recorder.h"
#include "base/pickle.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

class HistogramTest : public testing::Test {
 protected:
  const int32_t kAllocatorMemorySize = 64 << 10;  // 64 KiB

  HistogramTest() {}

  void SetUp() override {
    // Each test will have a clean state (no Histogram / BucketRanges
    // registered).
    InitializeStatisticsRecorder();
    // By getting the results-histogram before any persistent allocator
    // is attached, that histogram is guaranteed not to be stored in
    // any persistent memory segment (which simplifies some tests).
    GetCreateHistogramResultHistogram();
  }

  void TearDown() override {
    UninitializeStatisticsRecorder();
    DestroyPersistentMemoryAllocator();
  }

  void InitializeStatisticsRecorder() {
    statistics_recorder_ = new StatisticsRecorder();
  }

  void UninitializeStatisticsRecorder() {
    delete statistics_recorder_;
    statistics_recorder_ = NULL;
  }

  void CreatePersistentMemoryAllocator() {
    if (!allocator_memory_)
      allocator_memory_.reset(new char[kAllocatorMemorySize]);

    SetPersistentHistogramMemoryAllocator(nullptr);
    memset(allocator_memory_.get(), 0, kAllocatorMemorySize);
    SetPersistentHistogramMemoryAllocator(
        new PersistentMemoryAllocator(
            allocator_memory_.get(), kAllocatorMemorySize, 0,
            0, "HistogramAllocatorTest", false));
    allocator_ = GetPersistentHistogramMemoryAllocator();
  }

  void DestroyPersistentMemoryAllocator() {
    allocator_ = nullptr;
    SetPersistentHistogramMemoryAllocator(nullptr);
  }

  StatisticsRecorder* statistics_recorder_;
  scoped_ptr<char[]> allocator_memory_;
  PersistentMemoryAllocator* allocator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HistogramTest);
};

// Check for basic syntax and use.
TEST_F(HistogramTest, BasicTest) {
  // Try basic construction
  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10, HistogramBase::kNoFlags);
  EXPECT_TRUE(histogram);

  HistogramBase* linear_histogram = LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10, HistogramBase::kNoFlags);
  EXPECT_TRUE(linear_histogram);

  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  HistogramBase* custom_histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges, HistogramBase::kNoFlags);
  EXPECT_TRUE(custom_histogram);

  // Use standard macros (but with fixed samples)
  LOCAL_HISTOGRAM_TIMES("Test2Histogram", TimeDelta::FromDays(1));
  LOCAL_HISTOGRAM_COUNTS("Test3Histogram", 30);

  LOCAL_HISTOGRAM_ENUMERATION("Test6Histogram", 129, 130);
}

// Check for basic syntax and use.
TEST_F(HistogramTest, PersistentTest) {
  CreatePersistentMemoryAllocator();
  PersistentMemoryAllocator::MemoryInfo meminfo0;
  allocator_->GetMemoryInfo(&meminfo0);

  // Try basic construction
  HistogramBase* histogram = Histogram::FactoryGet(
      "TestHistogram", 1, 1000, 10,
      HistogramBase::kIsPersistent);
  EXPECT_TRUE(histogram);
  histogram->CheckName("TestHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo1;
  allocator_->GetMemoryInfo(&meminfo1);
  EXPECT_GT(meminfo0.free, meminfo1.free);

  HistogramBase* linear_histogram = LinearHistogram::FactoryGet(
      "TestLinearHistogram", 1, 1000, 10,
      HistogramBase::kIsPersistent);
  EXPECT_TRUE(linear_histogram);
  linear_histogram->CheckName("TestLinearHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo2;
  allocator_->GetMemoryInfo(&meminfo2);
  EXPECT_GT(meminfo1.free, meminfo2.free);

  HistogramBase* boolean_histogram = BooleanHistogram::FactoryGet(
      "TestBooleanHistogram", HistogramBase::kIsPersistent);
  EXPECT_TRUE(boolean_histogram);
  boolean_histogram->CheckName("TestBooleanHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo3;
  allocator_->GetMemoryInfo(&meminfo3);
  EXPECT_GT(meminfo2.free, meminfo3.free);

  std::vector<int> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(5);
  HistogramBase* custom_histogram = CustomHistogram::FactoryGet(
      "TestCustomHistogram", custom_ranges,
      HistogramBase::kIsPersistent);
  EXPECT_TRUE(custom_histogram);
  custom_histogram->CheckName("TestCustomHistogram");
  PersistentMemoryAllocator::MemoryInfo meminfo4;
  allocator_->GetMemoryInfo(&meminfo4);
  EXPECT_GT(meminfo3.free, meminfo4.free);

  PersistentMemoryAllocator::Iterator iter;
  uint32_t type;
  allocator_->CreateIterator(&iter);
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // Histogram
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // LinearHistogram
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // BooleanHistogram
  EXPECT_NE(0U, allocator_->GetNextIterable(&iter, &type));  // CustomHistogram
  EXPECT_EQ(0U, allocator_->GetNextIterable(&iter, &type));

  // Create a second allocator and have it access the memory of the first.
  scoped_ptr<HistogramBase> recovered;
  PersistentMemoryAllocator recovery(
      allocator_memory_.get(), kAllocatorMemorySize, 0,
      0, std::string(), false);
  recovery.CreateIterator(&iter);

  recovered.reset(GetNextPersistentHistogram(&recovery, &iter));
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestHistogram");

  recovered.reset(GetNextPersistentHistogram(&recovery, &iter));
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestLinearHistogram");

  recovered.reset(GetNextPersistentHistogram(&recovery, &iter));
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestBooleanHistogram");

  recovered.reset(GetNextPersistentHistogram(&recovery, &iter));
  ASSERT_TRUE(recovered);
  recovered->CheckName("TestCustomHistogram");

  recovered.reset(GetNextPersistentHistogram(&recovery, &iter));
  EXPECT_FALSE(recovered);

  // Use standard macros (but with fixed samples)
  LOCAL_HISTOGRAM_TIMES("Test2Histogram", TimeDelta::FromDays(1));
  LOCAL_HISTOGRAM_COUNTS("Test3Histogram", 30);
  LOCAL_HISTOGRAM_ENUMERATION("Test6Histogram", 129, 130);
}

// Check that the macro correctly matches histograms by name and records their
// data together.
TEST_F(HistogramTest, NameMatchTest) {
  LOCAL_HISTOGRAM_PERCENTAGE("DuplicatedHistogram", 10);
  LOCAL_HISTOGRAM_PERCENTAGE("DuplicatedHistogram", 10);
  HistogramBase* histogram = LinearHistogram::FactoryGet(
      "DuplicatedHistogram", 1, 101, 102, HistogramBase::kNoFlags);

  scoped_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(2, samples->GetCount(10));
}

TEST_F(HistogramTest, ExponentialRangesTest) {
  // Check that we got a nice exponential when there was enough room.
  BucketRanges ranges(9);
  Histogram::InitializeBucketRanges(1, 64, &ranges);
  EXPECT_EQ(0, ranges.range(0));
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    EXPECT_EQ(power_of_2, ranges.range(i));
    power_of_2 *= 2;
  }
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges.range(8));

  // Check the corresponding Histogram will use the correct ranges.
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("Histogram", 1, 64, 8, HistogramBase::kNoFlags));
  EXPECT_TRUE(ranges.Equals(histogram->bucket_ranges()));

  // When bucket count is limited, exponential ranges will partially look like
  // linear.
  BucketRanges ranges2(16);
  Histogram::InitializeBucketRanges(1, 32, &ranges2);

  EXPECT_EQ(0, ranges2.range(0));
  EXPECT_EQ(1, ranges2.range(1));
  EXPECT_EQ(2, ranges2.range(2));
  EXPECT_EQ(3, ranges2.range(3));
  EXPECT_EQ(4, ranges2.range(4));
  EXPECT_EQ(5, ranges2.range(5));
  EXPECT_EQ(6, ranges2.range(6));
  EXPECT_EQ(7, ranges2.range(7));
  EXPECT_EQ(9, ranges2.range(8));
  EXPECT_EQ(11, ranges2.range(9));
  EXPECT_EQ(14, ranges2.range(10));
  EXPECT_EQ(17, ranges2.range(11));
  EXPECT_EQ(21, ranges2.range(12));
  EXPECT_EQ(26, ranges2.range(13));
  EXPECT_EQ(32, ranges2.range(14));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges2.range(15));

  // Check the corresponding Histogram will use the correct ranges.
  Histogram* histogram2 = static_cast<Histogram*>(
      Histogram::FactoryGet("Histogram2", 1, 32, 15, HistogramBase::kNoFlags));
  EXPECT_TRUE(ranges2.Equals(histogram2->bucket_ranges()));
}

TEST_F(HistogramTest, LinearRangesTest) {
  BucketRanges ranges(9);
  LinearHistogram::InitializeBucketRanges(1, 7, &ranges);
  // Gets a nice linear set of bucket ranges.
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i, ranges.range(i));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges.range(8));

  // The correspoding LinearHistogram should use the correct ranges.
  Histogram* histogram = static_cast<Histogram*>(
      LinearHistogram::FactoryGet("Linear", 1, 7, 8, HistogramBase::kNoFlags));
  EXPECT_TRUE(ranges.Equals(histogram->bucket_ranges()));

  // Linear ranges are not divisible.
  BucketRanges ranges2(6);
  LinearHistogram::InitializeBucketRanges(1, 6, &ranges2);
  EXPECT_EQ(0, ranges2.range(0));
  EXPECT_EQ(1, ranges2.range(1));
  EXPECT_EQ(3, ranges2.range(2));
  EXPECT_EQ(4, ranges2.range(3));
  EXPECT_EQ(6, ranges2.range(4));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges2.range(5));
  // The correspoding LinearHistogram should use the correct ranges.
  Histogram* histogram2 = static_cast<Histogram*>(
      LinearHistogram::FactoryGet("Linear2", 1, 6, 5, HistogramBase::kNoFlags));
  EXPECT_TRUE(ranges2.Equals(histogram2->bucket_ranges()));
}

TEST_F(HistogramTest, ArrayToCustomRangesTest) {
  const HistogramBase::Sample ranges[3] = {5, 10, 20};
  std::vector<HistogramBase::Sample> ranges_vec =
      CustomHistogram::ArrayToCustomRanges(ranges, 3);
  ASSERT_EQ(6u, ranges_vec.size());
  EXPECT_EQ(5, ranges_vec[0]);
  EXPECT_EQ(6, ranges_vec[1]);
  EXPECT_EQ(10, ranges_vec[2]);
  EXPECT_EQ(11, ranges_vec[3]);
  EXPECT_EQ(20, ranges_vec[4]);
  EXPECT_EQ(21, ranges_vec[5]);
}

TEST_F(HistogramTest, CustomHistogramTest) {
  // A well prepared custom ranges.
  std::vector<HistogramBase::Sample> custom_ranges;
  custom_ranges.push_back(1);
  custom_ranges.push_back(2);

  Histogram* histogram = static_cast<Histogram*>(
      CustomHistogram::FactoryGet("TestCustomHistogram1", custom_ranges,
                                  HistogramBase::kNoFlags));
  const BucketRanges* ranges = histogram->bucket_ranges();
  ASSERT_EQ(4u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));  // Auto added.
  EXPECT_EQ(1, ranges->range(1));
  EXPECT_EQ(2, ranges->range(2));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(3));  // Auto added.

  // A unordered custom ranges.
  custom_ranges.clear();
  custom_ranges.push_back(2);
  custom_ranges.push_back(1);
  histogram = static_cast<Histogram*>(
      CustomHistogram::FactoryGet("TestCustomHistogram2", custom_ranges,
                                  HistogramBase::kNoFlags));
  ranges = histogram->bucket_ranges();
  ASSERT_EQ(4u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(1, ranges->range(1));
  EXPECT_EQ(2, ranges->range(2));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(3));

  // A custom ranges with duplicated values.
  custom_ranges.clear();
  custom_ranges.push_back(4);
  custom_ranges.push_back(1);
  custom_ranges.push_back(4);
  histogram = static_cast<Histogram*>(
      CustomHistogram::FactoryGet("TestCustomHistogram3", custom_ranges,
                                  HistogramBase::kNoFlags));
  ranges = histogram->bucket_ranges();
  ASSERT_EQ(4u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(1, ranges->range(1));
  EXPECT_EQ(4, ranges->range(2));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(3));
}

TEST_F(HistogramTest, CustomHistogramWithOnly2Buckets) {
  // This test exploits the fact that the CustomHistogram can have 2 buckets,
  // while the base class Histogram is *supposed* to have at least 3 buckets.
  // We should probably change the restriction on the base class (or not inherit
  // the base class!).

  std::vector<HistogramBase::Sample> custom_ranges;
  custom_ranges.push_back(4);

  Histogram* histogram = static_cast<Histogram*>(
      CustomHistogram::FactoryGet("2BucketsCustomHistogram", custom_ranges,
                                  HistogramBase::kNoFlags));
  const BucketRanges* ranges = histogram->bucket_ranges();
  ASSERT_EQ(3u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(4, ranges->range(1));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(2));
}

TEST_F(HistogramTest, AddCountTest) {
  const size_t kBucketCount = 50;
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("AddCountHistogram", 10, 100, kBucketCount,
                            HistogramBase::kNoFlags));

  histogram->AddCount(20, 15);
  histogram->AddCount(30, 14);

  scoped_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  EXPECT_EQ(29, samples->TotalCount());
  EXPECT_EQ(15, samples->GetCount(20));
  EXPECT_EQ(14, samples->GetCount(30));

  histogram->AddCount(20, 25);
  histogram->AddCount(30, 24);

  scoped_ptr<HistogramSamples> samples2 = histogram->SnapshotSamples();
  EXPECT_EQ(78, samples2->TotalCount());
  EXPECT_EQ(40, samples2->GetCount(20));
  EXPECT_EQ(38, samples2->GetCount(30));
}

TEST_F(HistogramTest, AddCount_LargeValuesDontOverflow) {
  const size_t kBucketCount = 50;
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("AddCountHistogram", 10, 1000000000, kBucketCount,
                            HistogramBase::kNoFlags));

  histogram->AddCount(200000000, 15);
  histogram->AddCount(300000000, 14);

  scoped_ptr<HistogramSamples> samples = histogram->SnapshotSamples();
  EXPECT_EQ(29, samples->TotalCount());
  EXPECT_EQ(15, samples->GetCount(200000000));
  EXPECT_EQ(14, samples->GetCount(300000000));

  histogram->AddCount(200000000, 25);
  histogram->AddCount(300000000, 24);

  scoped_ptr<HistogramSamples> samples2 = histogram->SnapshotSamples();
  EXPECT_EQ(78, samples2->TotalCount());
  EXPECT_EQ(40, samples2->GetCount(200000000));
  EXPECT_EQ(38, samples2->GetCount(300000000));
  EXPECT_EQ(19400000000LL, samples2->sum());
}

// Make sure histogram handles out-of-bounds data gracefully.
TEST_F(HistogramTest, BoundsTest) {
  const size_t kBucketCount = 50;
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("Bounded", 10, 100, kBucketCount,
                            HistogramBase::kNoFlags));

  // Put two samples "out of bounds" above and below.
  histogram->Add(5);
  histogram->Add(-50);

  histogram->Add(100);
  histogram->Add(10000);

  // Verify they landed in the underflow, and overflow buckets.
  scoped_ptr<SampleVector> samples = histogram->SnapshotSampleVector();
  EXPECT_EQ(2, samples->GetCountAtIndex(0));
  EXPECT_EQ(0, samples->GetCountAtIndex(1));
  size_t array_size = histogram->bucket_count();
  EXPECT_EQ(kBucketCount, array_size);
  EXPECT_EQ(0, samples->GetCountAtIndex(array_size - 2));
  EXPECT_EQ(2, samples->GetCountAtIndex(array_size - 1));

  std::vector<int> custom_ranges;
  custom_ranges.push_back(10);
  custom_ranges.push_back(50);
  custom_ranges.push_back(100);
  Histogram* test_custom_histogram = static_cast<Histogram*>(
      CustomHistogram::FactoryGet("TestCustomRangeBoundedHistogram",
                                  custom_ranges, HistogramBase::kNoFlags));

  // Put two samples "out of bounds" above and below.
  test_custom_histogram->Add(5);
  test_custom_histogram->Add(-50);
  test_custom_histogram->Add(100);
  test_custom_histogram->Add(1000);
  test_custom_histogram->Add(INT_MAX);

  // Verify they landed in the underflow, and overflow buckets.
  scoped_ptr<SampleVector> custom_samples =
      test_custom_histogram->SnapshotSampleVector();
  EXPECT_EQ(2, custom_samples->GetCountAtIndex(0));
  EXPECT_EQ(0, custom_samples->GetCountAtIndex(1));
  size_t bucket_count = test_custom_histogram->bucket_count();
  EXPECT_EQ(0, custom_samples->GetCountAtIndex(bucket_count - 2));
  EXPECT_EQ(3, custom_samples->GetCountAtIndex(bucket_count - 1));
}

// Check to be sure samples land as expected is "correct" buckets.
TEST_F(HistogramTest, BucketPlacementTest) {
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("Histogram", 1, 64, 8, HistogramBase::kNoFlags));

  // Add i+1 samples to the i'th bucket.
  histogram->Add(0);
  int power_of_2 = 1;
  for (int i = 1; i < 8; i++) {
    for (int j = 0; j <= i; j++)
      histogram->Add(power_of_2);
    power_of_2 *= 2;
  }

  // Check to see that the bucket counts reflect our additions.
  scoped_ptr<SampleVector> samples = histogram->SnapshotSampleVector();
  for (int i = 0; i < 8; i++)
    EXPECT_EQ(i + 1, samples->GetCountAtIndex(i));
}

TEST_F(HistogramTest, CorruptSampleCounts) {
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("Histogram", 1, 64, 8, HistogramBase::kNoFlags));

  // Add some samples.
  histogram->Add(20);
  histogram->Add(40);

  scoped_ptr<SampleVector> snapshot = histogram->SnapshotSampleVector();
  EXPECT_EQ(HistogramBase::NO_INCONSISTENCIES,
            histogram->FindCorruption(*snapshot));
  EXPECT_EQ(2, snapshot->redundant_count());
  EXPECT_EQ(2, snapshot->TotalCount());

  snapshot->counts_[3] += 100;  // Sample count won't match redundant count.
  EXPECT_EQ(HistogramBase::COUNT_LOW_ERROR,
            histogram->FindCorruption(*snapshot));
  snapshot->counts_[2] -= 200;
  EXPECT_EQ(HistogramBase::COUNT_HIGH_ERROR,
            histogram->FindCorruption(*snapshot));

  // But we can't spot a corruption if it is compensated for.
  snapshot->counts_[1] += 100;
  EXPECT_EQ(HistogramBase::NO_INCONSISTENCIES,
            histogram->FindCorruption(*snapshot));
}

TEST_F(HistogramTest, CorruptBucketBounds) {
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("Histogram", 1, 64, 8, HistogramBase::kNoFlags));

  scoped_ptr<HistogramSamples> snapshot = histogram->SnapshotSamples();
  EXPECT_EQ(HistogramBase::NO_INCONSISTENCIES,
            histogram->FindCorruption(*snapshot));

  BucketRanges* bucket_ranges =
      const_cast<BucketRanges*>(histogram->bucket_ranges());
  HistogramBase::Sample tmp = bucket_ranges->range(1);
  bucket_ranges->set_range(1, bucket_ranges->range(2));
  bucket_ranges->set_range(2, tmp);
  EXPECT_EQ(
      HistogramBase::BUCKET_ORDER_ERROR | HistogramBase::RANGE_CHECKSUM_ERROR,
      histogram->FindCorruption(*snapshot));

  bucket_ranges->set_range(2, bucket_ranges->range(1));
  bucket_ranges->set_range(1, tmp);
  EXPECT_EQ(0, histogram->FindCorruption(*snapshot));

  // Show that two simple changes don't offset each other
  bucket_ranges->set_range(3, bucket_ranges->range(3) + 1);
  EXPECT_EQ(HistogramBase::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(*snapshot));

  bucket_ranges->set_range(4, bucket_ranges->range(4) - 1);
  EXPECT_EQ(HistogramBase::RANGE_CHECKSUM_ERROR,
            histogram->FindCorruption(*snapshot));

  // Repair histogram so that destructor won't DCHECK().
  bucket_ranges->set_range(3, bucket_ranges->range(3) - 1);
  bucket_ranges->set_range(4, bucket_ranges->range(4) + 1);
}

TEST_F(HistogramTest, HistogramSerializeInfo) {
  Histogram* histogram = static_cast<Histogram*>(
      Histogram::FactoryGet("Histogram", 1, 64, 8,
                            HistogramBase::kIPCSerializationSourceFlag));
  Pickle pickle;
  histogram->SerializeInfo(&pickle);

  PickleIterator iter(pickle);

  int type;
  EXPECT_TRUE(iter.ReadInt(&type));
  EXPECT_EQ(HISTOGRAM, type);

  std::string name;
  EXPECT_TRUE(iter.ReadString(&name));
  EXPECT_EQ("Histogram", name);

  int flag;
  EXPECT_TRUE(iter.ReadInt(&flag));
  EXPECT_EQ(HistogramBase::kIPCSerializationSourceFlag, flag);

  int min;
  EXPECT_TRUE(iter.ReadInt(&min));
  EXPECT_EQ(1, min);

  int max;
  EXPECT_TRUE(iter.ReadInt(&max));
  EXPECT_EQ(64, max);

  uint32_t bucket_count;
  EXPECT_TRUE(iter.ReadUInt32(&bucket_count));
  EXPECT_EQ(8u, bucket_count);

  uint32_t checksum;
  EXPECT_TRUE(iter.ReadUInt32(&checksum));
  EXPECT_EQ(histogram->bucket_ranges()->checksum(), checksum);

  // No more data in the pickle.
  EXPECT_FALSE(iter.SkipBytes(1));
}

TEST_F(HistogramTest, CustomHistogramSerializeInfo) {
  std::vector<int> custom_ranges;
  custom_ranges.push_back(10);
  custom_ranges.push_back(100);

  HistogramBase* custom_histogram = CustomHistogram::FactoryGet(
      "TestCustomRangeBoundedHistogram",
      custom_ranges,
      HistogramBase::kNoFlags);
  Pickle pickle;
  custom_histogram->SerializeInfo(&pickle);

  // Validate the pickle.
  PickleIterator iter(pickle);

  int i;
  std::string s;
  uint32_t bucket_count;
  uint32_t ui32;
  EXPECT_TRUE(iter.ReadInt(&i) && iter.ReadString(&s) && iter.ReadInt(&i) &&
              iter.ReadInt(&i) && iter.ReadInt(&i) &&
              iter.ReadUInt32(&bucket_count) && iter.ReadUInt32(&ui32));
  EXPECT_EQ(3u, bucket_count);

  int range;
  EXPECT_TRUE(iter.ReadInt(&range));
  EXPECT_EQ(10, range);
  EXPECT_TRUE(iter.ReadInt(&range));
  EXPECT_EQ(100, range);

  // No more data in the pickle.
  EXPECT_FALSE(iter.SkipBytes(1));
}

TEST_F(HistogramTest, BadConstruction) {
  HistogramBase* histogram = Histogram::FactoryGet(
      "BadConstruction", 0, 100, 8, HistogramBase::kNoFlags);
  EXPECT_TRUE(histogram->HasConstructionArguments(1, 100, 8));

  // Try to get the same histogram name with different arguments.
  HistogramBase* bad_histogram = Histogram::FactoryGet(
      "BadConstruction", 0, 100, 7, HistogramBase::kNoFlags);
  EXPECT_EQ(NULL, bad_histogram);
  bad_histogram = Histogram::FactoryGet(
      "BadConstruction", 0, 99, 8, HistogramBase::kNoFlags);
  EXPECT_EQ(NULL, bad_histogram);

  HistogramBase* linear_histogram = LinearHistogram::FactoryGet(
      "BadConstructionLinear", 0, 100, 8, HistogramBase::kNoFlags);
  EXPECT_TRUE(linear_histogram->HasConstructionArguments(1, 100, 8));

  // Try to get the same histogram name with different arguments.
  bad_histogram = LinearHistogram::FactoryGet(
      "BadConstructionLinear", 0, 100, 7, HistogramBase::kNoFlags);
  EXPECT_EQ(NULL, bad_histogram);
  bad_histogram = LinearHistogram::FactoryGet(
      "BadConstructionLinear", 10, 100, 8, HistogramBase::kNoFlags);
  EXPECT_EQ(NULL, bad_histogram);
}

TEST_F(HistogramTest, FactoryTime) {
  const int kTestCreateCount = 1 << 14;  // Must be power-of-2.
  const int kTestLookupCount = 100000;

  // Create all histogram names in advance for accurate timing below.
  std::vector<std::string> histogram_names;
  for (int i = 0; i < kTestCreateCount; ++i) {
    histogram_names.push_back(
        StringPrintf("TestHistogram.%d", i % kTestCreateCount));
  }

  // Calculate cost of creating histograms.
  TimeTicks create_start = TimeTicks::Now();
  for (int i = 0; i < kTestCreateCount; ++i) {
    Histogram::FactoryGet(histogram_names[i], 0, 100, 10,
                          HistogramBase::kNoFlags);
  }
  TimeDelta create_ticks = TimeTicks::Now() - create_start;
  int64_t create_ms = create_ticks.InMilliseconds();

  LOG(INFO) << kTestCreateCount << " histogram creations took " << create_ms
            << "ms or about "
            << (create_ms * 1000000) / kTestCreateCount
            << "ns each.";

  // Calculate cost of looking up existing histograms.
  TimeTicks lookup_start = TimeTicks::Now();
  for (int i = 0; i < kTestLookupCount; ++i) {
    // 6007 is co-prime with kTestCreateCount and so will do lookups in an
    // order less likely to be cacheable (but still hit them all) should the
    // underlying storage use the exact histogram name as the key.
    const int i_mult = 6007;
    static_assert(i_mult < INT_MAX / kTestCreateCount, "Multiplier too big");
    int index = (i * i_mult) & (kTestCreateCount - 1);
    Histogram::FactoryGet(histogram_names[index], 0, 100, 10,
                          HistogramBase::kNoFlags);
  }
  TimeDelta lookup_ticks = TimeTicks::Now() - lookup_start;
  int64_t lookup_ms = lookup_ticks.InMilliseconds();

  LOG(INFO) << kTestLookupCount << " histogram lookups took " << lookup_ms
            << "ms or about "
            << (lookup_ms * 1000000) / kTestLookupCount
            << "ns each.";
}

#if GTEST_HAS_DEATH_TEST
// For Histogram, LinearHistogram and CustomHistogram, the minimum for a
// declared range is 1, while the maximum is (HistogramBase::kSampleType_MAX -
// 1). But we accept ranges exceeding those limits, and silently clamped to
// those limits. This is for backwards compatibility.
TEST(HistogramDeathTest, BadRangesTest) {
  HistogramBase* histogram = Histogram::FactoryGet(
      "BadRanges", 0, HistogramBase::kSampleType_MAX, 8,
      HistogramBase::kNoFlags);
  EXPECT_TRUE(
      histogram->HasConstructionArguments(
          1, HistogramBase::kSampleType_MAX - 1, 8));

  HistogramBase* linear_histogram = LinearHistogram::FactoryGet(
      "BadRangesLinear", 0, HistogramBase::kSampleType_MAX, 8,
      HistogramBase::kNoFlags);
  EXPECT_TRUE(
      linear_histogram->HasConstructionArguments(
          1, HistogramBase::kSampleType_MAX - 1, 8));

  std::vector<int> custom_ranges;
  custom_ranges.push_back(0);
  custom_ranges.push_back(5);
  Histogram* custom_histogram = static_cast<Histogram*>(
      CustomHistogram::FactoryGet(
          "BadRangesCustom", custom_ranges, HistogramBase::kNoFlags));
  const BucketRanges* ranges = custom_histogram->bucket_ranges();
  ASSERT_EQ(3u, ranges->size());
  EXPECT_EQ(0, ranges->range(0));
  EXPECT_EQ(5, ranges->range(1));
  EXPECT_EQ(HistogramBase::kSampleType_MAX, ranges->range(2));

  // CustomHistogram does not accepts kSampleType_MAX as range.
  custom_ranges.push_back(HistogramBase::kSampleType_MAX);
  EXPECT_DEATH(CustomHistogram::FactoryGet("BadRangesCustom2", custom_ranges,
                                           HistogramBase::kNoFlags),
               "");

  // CustomHistogram needs at least 1 valid range.
  custom_ranges.clear();
  custom_ranges.push_back(0);
  EXPECT_DEATH(CustomHistogram::FactoryGet("BadRangesCustom3", custom_ranges,
                                           HistogramBase::kNoFlags),
               "");
}
#endif

}  // namespace base
