// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test of classes in the tracked_objects.h classes.

#include "base/tracked_objects.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracked_objects {

class TrackedObjectsTest : public testing::Test {
 public:
  TrackedObjectsTest() {
    ThreadData::ShutdownSingleThreadedCleanup();
  }

  ~TrackedObjectsTest() {
    ThreadData::ShutdownSingleThreadedCleanup();
  }
};

TEST_F(TrackedObjectsTest, MinimalStartupShutdown) {
  // Minimal test doesn't even create any tasks.
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  ThreadData* data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  EXPECT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(0u, birth_map.size());
  ThreadData::DeathMap death_map;
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());
  ThreadData::ShutdownSingleThreadedCleanup();

  // Do it again, just to be sure we reset state completely.
  ThreadData::InitializeAndSetTrackingStatus(true);
  EXPECT_FALSE(ThreadData::first());  // No activity even on this thread.
  data = ThreadData::Get();
  EXPECT_TRUE(ThreadData::first());  // Now class was constructed.
  EXPECT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::Get());
  birth_map.clear();
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(0u, birth_map.size());
  death_map.clear();
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());
}

TEST_F(TrackedObjectsTest, TinyStartupShutdown) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  // Instigate tracking on a single tracked object, on our thread.
  const Location& location = FROM_HERE;
  ThreadData::TallyABirthIfActive(location);

  const ThreadData* data = ThreadData::first();
  ASSERT_TRUE(data);
  EXPECT_TRUE(!data->next());
  EXPECT_EQ(data, ThreadData::Get());
  ThreadData::BirthMap birth_map;
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(1, birth_map.begin()->second->birth_count());  // 1 birth.
  ThreadData::DeathMap death_map;
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(0u, death_map.size());                         // No deaths.


  // Now instigate another birth, and a first death at the same location.
  // TrackingInfo will call TallyABirth() during construction.
  base::TimeTicks kBogusStartTime;
  base::TrackingInfo pending_task(location, kBogusStartTime);
  TrackedTime kBogusStartRunTime;
  TrackedTime kBogusEndRunTime;
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task, kBogusStartRunTime,
                                              kBogusEndRunTime);

  birth_map.clear();
  data->SnapshotBirthMap(&birth_map);
  EXPECT_EQ(1u, birth_map.size());                         // 1 birth location.
  EXPECT_EQ(2, birth_map.begin()->second->birth_count());  // 2 births.
  death_map.clear();
  data->SnapshotDeathMap(&death_map);
  EXPECT_EQ(1u, death_map.size());                         // 1 location.
  EXPECT_EQ(1, death_map.begin()->second.count());         // 1 death.

  // The births were at the same location as the one known death.
  EXPECT_EQ(birth_map.begin()->second, death_map.begin()->first);
}

TEST_F(TrackedObjectsTest, DeathDataTest) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  scoped_ptr<DeathData> data(new DeathData());
  ASSERT_NE(data, reinterpret_cast<DeathData*>(NULL));
  EXPECT_EQ(data->run_duration(), Duration());
  EXPECT_EQ(data->queue_duration(), Duration());
  EXPECT_EQ(data->AverageMsRunDuration(), 0);
  EXPECT_EQ(data->AverageMsQueueDuration(), 0);
  EXPECT_EQ(data->count(), 0);

  int run_ms = 42;
  int queue_ms = 8;

  Duration run_duration = Duration().FromMilliseconds(run_ms);
  Duration queue_duration = Duration().FromMilliseconds(queue_ms);
  data->RecordDeath(queue_duration, run_duration);
  EXPECT_EQ(data->run_duration(), run_duration);
  EXPECT_EQ(data->queue_duration(), queue_duration);
  EXPECT_EQ(data->AverageMsRunDuration(), run_ms);
  EXPECT_EQ(data->AverageMsQueueDuration(), queue_ms);
  EXPECT_EQ(data->count(), 1);

  data->RecordDeath(queue_duration, run_duration);
  EXPECT_EQ(data->run_duration(), run_duration + run_duration);
  EXPECT_EQ(data->queue_duration(), queue_duration + queue_duration);
  EXPECT_EQ(data->AverageMsRunDuration(), run_ms);
  EXPECT_EQ(data->AverageMsQueueDuration(), queue_ms);
  EXPECT_EQ(data->count(), 2);

  scoped_ptr<base::DictionaryValue> dictionary(data->ToValue());
  int integer;
  EXPECT_TRUE(dictionary->GetInteger("run_ms", &integer));
  EXPECT_EQ(integer, 2 * run_ms);
  EXPECT_TRUE(dictionary->GetInteger("queue_ms", &integer));
  EXPECT_EQ(integer, 2* queue_ms);
  EXPECT_TRUE(dictionary->GetInteger("count", &integer));
  EXPECT_EQ(integer, 2);

  std::string output;
  data->WriteHTML(&output);
  std::string results = "Lives:2, Run:84ms(42ms/life) Queue:16ms(8ms/life) ";
  EXPECT_EQ(output, results);

  scoped_ptr<base::Value> value(data->ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{\"count\":2,\"queue_ms\":16,\"run_ms\":84}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, DeactivatedBirthOnlyToValueWorkerThread) {
  // Transition to Deactivated state before doing anything.
  if (!ThreadData::InitializeAndSetTrackingStatus(false))
    return;
  // We don't initialize system with a thread name, so we're viewed as a worker
  // thread.
  const int kFakeLineNumber = 173;
  const char* kFile = "FixedFileName";
  const char* kFunction = "BirthOnlyToValueWorkerThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  Births* birth = ThreadData::TallyABirthIfActive(location);
  // We should now see a NULL birth record.
  EXPECT_EQ(birth, reinterpret_cast<Births*>(NULL));

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{"
      "\"list\":["
      "]"
    "}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, DeactivatedBirthOnlyToValueMainThread) {
  // Start in the deactivated state.
  if (!ThreadData::InitializeAndSetTrackingStatus(false))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeMainThreadName");
  const int kFakeLineNumber = 173;
  const char* kFile = "FixedFileName";
  const char* kFunction = "BirthOnlyToValueMainThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  // We expect to not get a birth record.
  EXPECT_EQ(birth, reinterpret_cast<Births*>(NULL));

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{"
      "\"list\":["
      "]"
    "}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, BirthOnlyToValueWorkerThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;
  // We don't initialize system with a thread name, so we're viewed as a worker
  // thread.
  const int kFakeLineNumber = 173;
  const char* kFile = "FixedFileName";
  const char* kFunction = "BirthOnlyToValueWorkerThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"WorkerThread-1\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":0,"
            "\"run_ms\":0"
          "},"
          "\"death_thread\":\"Still_Alive\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"BirthOnlyToValueWorkerThread\","
            "\"line_number\":173"
          "}"
        "}"
      "]"
    "}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, BirthOnlyToValueMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeMainThreadName");
  const int kFakeLineNumber = 173;
  const char* kFile = "FixedFileName";
  const char* kFunction = "BirthOnlyToValueMainThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string birth_only_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeMainThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":0,"
            "\"run_ms\":0"
          "},"
          "\"death_thread\":\"Still_Alive\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"BirthOnlyToValueMainThread\","
            "\"line_number\":173"
          "}"
        "}"
      "]"
    "}";
  EXPECT_EQ(json, birth_only_result);
}

TEST_F(TrackedObjectsTest, LifeCycleToValueMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeMainThreadName");
  const int kFakeLineNumber = 236;
  const char* kFile = "FixedFileName";
  const char* kFunction = "LifeCycleToValueMainThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  const base::TimeTicks kTimePosted = base::TimeTicks()
      + base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeMainThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":4,"
            "\"run_ms\":2"
          "},"
          "\"death_thread\":\"SomeMainThreadName\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"LifeCycleToValueMainThread\","
            "\"line_number\":236"
          "}"
        "}"
      "]"
    "}";
  EXPECT_EQ(one_line_result, json);
}

// We will deactivate tracking after the birth, and before the death, and
// demonstrate that the lifecycle is completely tallied. This ensures that
// our tallied births are matched by tallied deaths (except for when the
// task is still running, or is queued).
TEST_F(TrackedObjectsTest, LifeCycleMidDeactivatedToValueMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeMainThreadName");
  const int kFakeLineNumber = 236;
  const char* kFile = "FixedFileName";
  const char* kFunction = "LifeCycleToValueMainThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  const base::TimeTicks kTimePosted = base::TimeTicks()
      + base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  // Turn off tracking now that we have births.
  EXPECT_TRUE(ThreadData::InitializeAndSetTrackingStatus(false));

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeMainThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":4,"
            "\"run_ms\":2"
          "},"
          "\"death_thread\":\"SomeMainThreadName\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"LifeCycleToValueMainThread\","
            "\"line_number\":236"
          "}"
        "}"
      "]"
    "}";
  EXPECT_EQ(one_line_result, json);
}

// We will deactivate tracking before starting a life cycle, and neither
// the birth nor the death will be recorded.
TEST_F(TrackedObjectsTest, LifeCyclePreDeactivatedToValueMainThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(false))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeMainThreadName");
  const int kFakeLineNumber = 236;
  const char* kFile = "FixedFileName";
  const char* kFunction = "LifeCycleToValueMainThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_EQ(birth, reinterpret_cast<Births*>(NULL));

  const base::TimeTicks kTimePosted = base::TimeTicks()
      + base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
      "]"
    "}";
  EXPECT_EQ(one_line_result, json);
}

TEST_F(TrackedObjectsTest, LifeCycleToValueWorkerThread) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  // Don't initialize thread, so that we appear as a worker thread.
  // ThreadData::InitializeThreadContext("SomeMainThreadName");

  const int kFakeLineNumber = 236;
  const char* kFile = "FixedFileName";
  const char* kFunction = "LifeCycleToValueWorkerThread";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));

  const TrackedTime kTimePosted = TrackedTime() + Duration::FromMilliseconds(1);
  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnWorkerThreadIfTracking(birth, kTimePosted,
      kStartOfRun, kEndOfRun);

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"WorkerThread-1\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":4,"
            "\"run_ms\":2"
          "},"
          "\"death_thread\":\"WorkerThread-1\","
          "\"location\":{"
            "\"file_name\":\"FixedFileName\","
            "\"function_name\":\"LifeCycleToValueWorkerThread\","
            "\"line_number\":236"
          "}"
        "}"
      "]"
    "}";
  EXPECT_EQ(one_line_result, json);
}

TEST_F(TrackedObjectsTest, TwoLives) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeFileThreadName");
  const int kFakeLineNumber = 222;
  const char* kFile = "AnotherFileName";
  const char* kFunction = "TwoLives";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);
  // Do not delete birth.  We don't own it.
  Births* birth = ThreadData::TallyABirthIfActive(location);
  EXPECT_NE(birth, reinterpret_cast<Births*>(NULL));


  const base::TimeTicks kTimePosted = base::TimeTicks()
      + base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task2(location, kDelayedStartTime);
  pending_task2.time_posted = kTimePosted;  // Overwrite implied Now().

  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task2,
      kStartOfRun, kEndOfRun);

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeFileThreadName\","
          "\"death_data\":{"
            "\"count\":2,"
            "\"queue_ms\":8,"
            "\"run_ms\":4"
          "},"
          "\"death_thread\":\"SomeFileThreadName\","
          "\"location\":{"
            "\"file_name\":\"AnotherFileName\","
            "\"function_name\":\"TwoLives\","
            "\"line_number\":222"
          "}"
        "}"
      "]"
    "}";
  EXPECT_EQ(one_line_result, json);
}

TEST_F(TrackedObjectsTest, DifferentLives) {
  if (!ThreadData::InitializeAndSetTrackingStatus(true))
    return;

  // Use a well named thread.
  ThreadData::InitializeThreadContext("SomeFileThreadName");
  const int kFakeLineNumber = 567;
  const char* kFile = "AnotherFileName";
  const char* kFunction = "DifferentLives";
  Location location(kFunction, kFile, kFakeLineNumber, NULL);

  const base::TimeTicks kTimePosted = base::TimeTicks()
      + base::TimeDelta::FromMilliseconds(1);
  const base::TimeTicks kDelayedStartTime = base::TimeTicks();
  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task(location, kDelayedStartTime);
  pending_task.time_posted = kTimePosted;  // Overwrite implied Now().

  const TrackedTime kStartOfRun = TrackedTime() +
      Duration::FromMilliseconds(5);
  const TrackedTime kEndOfRun = TrackedTime() + Duration::FromMilliseconds(7);
  ThreadData::TallyRunOnNamedThreadIfTracking(pending_task,
      kStartOfRun, kEndOfRun);

  const int kSecondFakeLineNumber = 999;
  Location second_location(kFunction, kFile, kSecondFakeLineNumber, NULL);

  // TrackingInfo will call TallyABirth() during construction.
  base::TrackingInfo pending_task2(second_location, kDelayedStartTime);
  pending_task2.time_posted = kTimePosted;  // Overwrite implied Now().

  scoped_ptr<base::Value> value(ThreadData::ToValue());
  std::string json;
  base::JSONWriter::Write(value.get(), false, &json);
  std::string one_line_result = "{"
      "\"list\":["
        "{"
          "\"birth_thread\":\"SomeFileThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":4,"
            "\"run_ms\":2"
          "},"
          "\"death_thread\":\"SomeFileThreadName\","
          "\"location\":{"
            "\"file_name\":\"AnotherFileName\","
            "\"function_name\":\"DifferentLives\","
            "\"line_number\":567"
          "}"
        "},"
        "{"
          "\"birth_thread\":\"SomeFileThreadName\","
          "\"death_data\":{"
            "\"count\":1,"
            "\"queue_ms\":0,"
            "\"run_ms\":0"
          "},"
          "\"death_thread\":\"Still_Alive\","
          "\"location\":{"
            "\"file_name\":\"AnotherFileName\","
            "\"function_name\":\"DifferentLives\","
            "\"line_number\":999"
          "}"
        "}"
      "]"
    "}";
  EXPECT_EQ(one_line_result, json);
}

}  // namespace tracked_objects
