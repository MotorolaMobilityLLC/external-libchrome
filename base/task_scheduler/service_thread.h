// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SCHEDULER_SERVICE_THREAD_H_
#define BASE_TASK_SCHEDULER_SERVICE_THREAD_H_

#include "base/base_export.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"

namespace base {
namespace internal {

class TaskTracker;

// The TaskScheduler's ServiceThread is a mostly idle thread that is responsible
// for handling async events (e.g. delayed tasks and async I/O). Its role is to
// merely forward such events to their destination (hence staying mostly idle
// and highly responsive).
// It aliases Thread::Run() to enforce that ServiceThread::Run() be on the stack
// and make it easier to identify the service thread in stack traces.
class BASE_EXPORT ServiceThread : public Thread {
 public:
  // Constructs a ServiceThread which will report latency metrics through
  // |task_tracker| if non-null. In that case, this ServiceThread will assume a
  // registered TaskScheduler instance and that |task_tracker| will outlive this
  // ServiceThread.
  explicit ServiceThread(const TaskTracker* task_tracker);

 private:
  // Thread:
  void Init() override;
  void Run(RunLoop* run_loop) override;

  // Kicks off async tasks which will record a histogram on the latency of
  // various traits.
  void PerformHeartbeatLatencyReport() const;

  const TaskTracker* const task_tracker_;

  // Fires a recurring heartbeat task to record latency histograms which are
  // independent from any execution sequence. This is done on the service thread
  // to avoid all external dependencies (even main thread).
  base::RepeatingTimer heartbeat_latency_timer_;

  DISALLOW_COPY_AND_ASSIGN(ServiceThread);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_SCHEDULER_SERVICE_THREAD_H_
