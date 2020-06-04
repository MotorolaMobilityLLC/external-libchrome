// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/test_utils.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/synchronization/waitable_event.h"

namespace mojo {
namespace system {
namespace test {

namespace {

void PostTaskAndWaitHelper(base::WaitableEvent* event,
                           const base::Closure& task) {
  task.Run();
  event->Signal();
}

}  // namespace

void PostTaskAndWait(scoped_refptr<base::TaskRunner> task_runner,
                     const tracked_objects::Location& from_here,
                     const base::Closure& task) {
  base::WaitableEvent event(false, false);
  task_runner->PostTask(from_here,
                        base::Bind(&PostTaskAndWaitHelper, &event, task));
  event.Wait();
}

// TestIOThread ----------------------------------------------------------------

TestIOThread::TestIOThread(Mode mode)
    : io_thread_("test_io_thread"),
      io_thread_started_(false) {
  switch (mode) {
    case kAutoStart:
      Start();
      return;
    case kManualStart:
      return;
  }
  CHECK(false) << "Invalid mode";
}

TestIOThread::~TestIOThread() {
  Stop();
}

void TestIOThread::Start() {
  CHECK(!io_thread_started_);
  io_thread_started_ = true;
  CHECK(io_thread_.StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0)));
}

void TestIOThread::Stop() {
  // Note: It's okay to call |Stop()| even if the thread isn't running.
  io_thread_.Stop();
  io_thread_started_ = false;
}

void TestIOThread::PostTask(const tracked_objects::Location& from_here,
                            const base::Closure& task) {
  task_runner()->PostTask(from_here, task);
}

void TestIOThread::PostTaskAndWait(const tracked_objects::Location& from_here,
                                   const base::Closure& task) {
  ::mojo::system::test::PostTaskAndWait(task_runner(), from_here, task);
}

}  // namespace test
}  // namespace system
}  // namespace mojo
