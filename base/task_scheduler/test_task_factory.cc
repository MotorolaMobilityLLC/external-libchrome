// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task_scheduler/test_task_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/synchronization/waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {
namespace test {

TestTaskFactory::TestTaskFactory(scoped_refptr<TaskRunner> task_runner,
                                 ExecutionMode execution_mode)
    : cv_(&lock_),
      task_runner_(std::move(task_runner)),
      execution_mode_(execution_mode) {
  // Detach |thread_checker_| from the current thread. It will be attached to
  // the first thread that calls ThreadCheckerImpl::CalledOnValidThread().
  thread_checker_.DetachFromThread();
}

TestTaskFactory::~TestTaskFactory() {
  WaitForAllTasksToRun();
}

bool TestTaskFactory::PostTask(PostNestedTask post_nested_task,
                               WaitableEvent* event) {
  AutoLock auto_lock(lock_);
  return task_runner_->PostTask(
      FROM_HERE,
      Bind(&TestTaskFactory::RunTaskCallback, Unretained(this),
           num_posted_tasks_++, post_nested_task, Unretained(event)));
}

void TestTaskFactory::WaitForAllTasksToRun() const {
  AutoLock auto_lock(lock_);
  while (ran_tasks_.size() < num_posted_tasks_)
    cv_.Wait();
}

void TestTaskFactory::RunTaskCallback(size_t task_index,
                                      PostNestedTask post_nested_task,
                                      WaitableEvent* event) {
  if (post_nested_task == PostNestedTask::YES)
    PostTask(PostNestedTask::NO, nullptr);

  EXPECT_TRUE(task_runner_->RunsTasksOnCurrentThread());

  {
    AutoLock auto_lock(lock_);

    DCHECK_LE(task_index, num_posted_tasks_);

    if ((execution_mode_ == ExecutionMode::SINGLE_THREADED ||
         execution_mode_ == ExecutionMode::SEQUENCED) &&
        task_index != ran_tasks_.size()) {
      ADD_FAILURE() << "A task didn't run in the expected order.";
    }

    if (execution_mode_ == ExecutionMode::SINGLE_THREADED)
      EXPECT_TRUE(thread_checker_.CalledOnValidThread());

    if (ran_tasks_.find(task_index) != ran_tasks_.end())
      ADD_FAILURE() << "A task ran more than once.";
    ran_tasks_.insert(task_index);

    cv_.Signal();
  }

  if (event)
    event->Wait();
}

}  // namespace test
}  // namespace internal
}  // namespace base
