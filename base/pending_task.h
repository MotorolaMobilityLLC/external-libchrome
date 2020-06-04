// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PENDING_TASK_H_
#define PENDING_TASK_H_
#pragma once

#include <queue>

#include "base/callback.h"
#include "base/location.h"
#include "base/time.h"
#include "base/tracking_info.h"

namespace base {

// Contains data about a pending task. Stored in TaskQueue and DelayedTaskQueue
// for use by classes that queue and execute tasks.
struct PendingTask : public TrackingInfo {
  PendingTask(const tracked_objects::Location& posted_from,
              const Closure& task);
  PendingTask(const tracked_objects::Location& posted_from,
              const Closure& task,
              TimeTicks delayed_run_time,
              bool nestable);
  ~PendingTask();

  // Used to support sorting.
  bool operator<(const PendingTask& other) const;

  // The task to run.
  Closure task;

  // The site this PendingTask was posted from.
  tracked_objects::Location posted_from;

  // Secondary sort key for run time.
  int sequence_num;

  // OK to dispatch from a nested loop.
  bool nestable;
};

// Wrapper around std::queue specialized for PendingTask which adds a Swap
// helper method.
class TaskQueue : public std::queue<PendingTask> {
 public:
  void Swap(TaskQueue* queue);
};

}  // namespace base

#endif  // PENDING_TASK_H_
