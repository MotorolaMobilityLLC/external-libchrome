// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/worker_pool_posix.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/stringprintf.h"
#include "base/task.h"
#include "base/threading/platform_thread.h"
#include "base/threading/worker_pool.h"
#include "base/tracked_objects.h"

namespace base {

namespace {

const int kIdleSecondsBeforeExit = 10 * 60;
// A stack size of 64 KB is too small for the CERT_PKIXVerifyCert
// function of NSS because of NSS bug 439169.
const int kWorkerThreadStackSize = 128 * 1024;

class WorkerPoolImpl {
 public:
  WorkerPoolImpl();
  ~WorkerPoolImpl();

  void PostTask(const tracked_objects::Location& from_here, Task* task,
                bool task_is_slow);
  void PostTask(const tracked_objects::Location& from_here,
                const base::Closure& task, bool task_is_slow);

 private:
  scoped_refptr<base::PosixDynamicThreadPool> pool_;
};

WorkerPoolImpl::WorkerPoolImpl()
    : pool_(new base::PosixDynamicThreadPool("WorkerPool",
                                             kIdleSecondsBeforeExit)) {
}

WorkerPoolImpl::~WorkerPoolImpl() {
  pool_->Terminate();
}

void WorkerPoolImpl::PostTask(const tracked_objects::Location& from_here,
                              Task* task, bool task_is_slow) {
  pool_->PostTask(from_here, task);
}

void WorkerPoolImpl::PostTask(const tracked_objects::Location& from_here,
                              const base::Closure& task, bool task_is_slow) {
  pool_->PostTask(from_here, task);
}

base::LazyInstance<WorkerPoolImpl> g_lazy_worker_pool(base::LINKER_INITIALIZED);

class WorkerThread : public PlatformThread::Delegate {
 public:
  WorkerThread(const std::string& name_prefix,
               base::PosixDynamicThreadPool* pool)
      : name_prefix_(name_prefix),
        pool_(pool) {}

  virtual void ThreadMain();

 private:
  const std::string name_prefix_;
  scoped_refptr<base::PosixDynamicThreadPool> pool_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThread);
};

void WorkerThread::ThreadMain() {
  const std::string name = base::StringPrintf(
      "%s/%d", name_prefix_.c_str(), PlatformThread::CurrentId());
  PlatformThread::SetName(name.c_str());

  for (;;) {
    PosixDynamicThreadPool::PendingTask pending_task = pool_->WaitForTask();
    if (pending_task.task.is_null())
      break;
    UNSHIPPED_TRACE_EVENT2("task", "WorkerThread::ThreadMain::Run",
        "src_file", pending_task.posted_from.file_name(),
        "src_func", pending_task.posted_from.function_name());
    pending_task.task.Run();
  }

  // The WorkerThread is non-joinable, so it deletes itself.
  delete this;
}

}  // namespace

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          Task* task, bool task_is_slow) {
  g_lazy_worker_pool.Pointer()->PostTask(from_here, task, task_is_slow);
  return true;
}

bool WorkerPool::PostTask(const tracked_objects::Location& from_here,
                          const base::Closure& task, bool task_is_slow) {
  g_lazy_worker_pool.Pointer()->PostTask(from_here, task, task_is_slow);
  return true;
}

PosixDynamicThreadPool::PendingTask::PendingTask(
    const tracked_objects::Location& posted_from,
    const base::Closure& task)
    : posted_from(posted_from),
      task(task) {
}

PosixDynamicThreadPool::PendingTask::~PendingTask() {
}

PosixDynamicThreadPool::PosixDynamicThreadPool(
    const std::string& name_prefix,
    int idle_seconds_before_exit)
    : name_prefix_(name_prefix),
      idle_seconds_before_exit_(idle_seconds_before_exit),
      pending_tasks_available_cv_(&lock_),
      num_idle_threads_(0),
      terminated_(false),
      num_idle_threads_cv_(NULL) {}

PosixDynamicThreadPool::~PosixDynamicThreadPool() {
  while (!pending_tasks_.empty()) {
    PendingTask pending_task = pending_tasks_.front();
    pending_tasks_.pop();
  }
}

void PosixDynamicThreadPool::Terminate() {
  {
    AutoLock locked(lock_);
    DCHECK(!terminated_) << "Thread pool is already terminated.";
    terminated_ = true;
  }
  pending_tasks_available_cv_.Broadcast();
}

void PosixDynamicThreadPool::PostTask(
    const tracked_objects::Location& from_here,
    Task* task) {
  PendingTask pending_task(from_here,
                           base::Bind(&subtle::TaskClosureAdapter::Run,
                                      new subtle::TaskClosureAdapter(task)));
  // |pending_task| and AddTask() work in conjunction here to ensure that after
  // a successful AddTask(), the TaskClosureAdapter object is deleted on the
  // worker thread. In AddTask(), the reference |pending_task.task| is handed
  // off in a destructive manner to ensure that the local copy of
  // |pending_task| doesn't keep a ref on the Closure causing the
  // TaskClosureAdapter to be deleted on the wrong thread.
  AddTask(&pending_task);
}

void PosixDynamicThreadPool::PostTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task) {
  PendingTask pending_task(from_here, task);
  AddTask(&pending_task);
}

void PosixDynamicThreadPool::AddTask(PendingTask* pending_task) {
  AutoLock locked(lock_);
  DCHECK(!terminated_) <<
      "This thread pool is already terminated.  Do not post new tasks.";

  pending_tasks_.push(*pending_task);
  pending_task->task.Reset();

  // We have enough worker threads.
  if (static_cast<size_t>(num_idle_threads_) >= pending_tasks_.size()) {
    pending_tasks_available_cv_.Signal();
  } else {
    // The new PlatformThread will take ownership of the WorkerThread object,
    // which will delete itself on exit.
    WorkerThread* worker =
        new WorkerThread(name_prefix_, this);
    PlatformThread::CreateNonJoinable(kWorkerThreadStackSize, worker);
  }
}

PosixDynamicThreadPool::PendingTask PosixDynamicThreadPool::WaitForTask() {
  AutoLock locked(lock_);

  if (terminated_)
    return PendingTask(FROM_HERE, base::Closure());

  if (pending_tasks_.empty()) {  // No work available, wait for work.
    num_idle_threads_++;
    if (num_idle_threads_cv_.get())
      num_idle_threads_cv_->Signal();
    pending_tasks_available_cv_.TimedWait(
        TimeDelta::FromSeconds(idle_seconds_before_exit_));
    num_idle_threads_--;
    if (num_idle_threads_cv_.get())
      num_idle_threads_cv_->Signal();
    if (pending_tasks_.empty()) {
      // We waited for work, but there's still no work.  Return NULL to signal
      // the thread to terminate.
      return PendingTask(FROM_HERE, base::Closure());
    }
  }

  PendingTask pending_task = pending_tasks_.front();
  pending_tasks_.pop();
  return pending_task;
}

}  // namespace base
