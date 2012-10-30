// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/threading/platform_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

// The message loops on which each waitable event timer should be tested.
const MessageLoop::Type testing_message_loops[] = {
  MessageLoop::TYPE_DEFAULT,
  MessageLoop::TYPE_IO,
#if !defined(OS_IOS)  // iOS does not allow direct running of the UI loop.
  MessageLoop::TYPE_UI,
#endif
};

const int kNumTestingMessageLoops = arraysize(testing_message_loops);

class QuitDelegate : public WaitableEventWatcher::Delegate {
 public:
  virtual void OnWaitableEventSignaled(WaitableEvent* event) OVERRIDE {
    MessageLoop::current()->Quit();
  }
};

class DecrementCountDelegate : public WaitableEventWatcher::Delegate {
 public:
  explicit DecrementCountDelegate(int* counter) : counter_(counter) {
  }
  virtual void OnWaitableEventSignaled(WaitableEvent* object) OVERRIDE {
    --(*counter_);
  }
 private:
  int* counter_;
};

void RunTest_BasicSignal(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(true, false);

  WaitableEventWatcher watcher;
  EXPECT_TRUE(watcher.GetWatchedEvent() == NULL);

  QuitDelegate delegate;
  watcher.StartWatching(&event, &delegate);
  EXPECT_EQ(&event, watcher.GetWatchedEvent());

  event.Signal();

  MessageLoop::current()->Run();

  EXPECT_TRUE(watcher.GetWatchedEvent() == NULL);
}

void RunTest_BasicCancel(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(true, false);

  WaitableEventWatcher watcher;

  QuitDelegate delegate;
  watcher.StartWatching(&event, &delegate);

  watcher.StopWatching();
}

void RunTest_CancelAfterSet(MessageLoop::Type message_loop_type) {
  MessageLoop message_loop(message_loop_type);

  // A manual-reset event that is not yet signaled.
  WaitableEvent event(true, false);

  WaitableEventWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);

  watcher.StartWatching(&event, &delegate);

  event.Signal();

  // Let the background thread do its business
  base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(30));

  watcher.StopWatching();

  MessageLoop::current()->RunUntilIdle();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);
}

void RunTest_OutlivesMessageLoop(MessageLoop::Type message_loop_type) {
  // Simulate a MessageLoop that dies before an WaitableEventWatcher.  This
  // ordinarily doesn't happen when people use the Thread class, but it can
  // happen when people use the Singleton pattern or atexit.
  WaitableEvent event(true, false);
  {
    WaitableEventWatcher watcher;
    {
      MessageLoop message_loop(message_loop_type);

      QuitDelegate delegate;
      watcher.StartWatching(&event, &delegate);
    }
  }
}

void RunTest_DeleteUnder(MessageLoop::Type message_loop_type) {
  // Delete the WaitableEvent out from under the Watcher. This is explictly
  // allowed by the interface.

  MessageLoop message_loop(message_loop_type);

  {
    WaitableEventWatcher watcher;

    WaitableEvent* event = new WaitableEvent(false, false);
    QuitDelegate delegate;
    watcher.StartWatching(event, &delegate);
    delete event;
  }
}

}  // namespace

//-----------------------------------------------------------------------------

TEST(WaitableEventWatcherTest, BasicSignal) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_BasicSignal(testing_message_loops[i]);
  }
}

TEST(WaitableEventWatcherTest, BasicCancel) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_BasicCancel(testing_message_loops[i]);
  }
}

TEST(WaitableEventWatcherTest, CancelAfterSet) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_CancelAfterSet(testing_message_loops[i]);
  }
}

TEST(WaitableEventWatcherTest, OutlivesMessageLoop) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_OutlivesMessageLoop(testing_message_loops[i]);
  }
}

#if defined(OS_WIN)
// Crashes sometimes on vista.  http://crbug.com/62119
#define MAYBE_DeleteUnder DISABLED_DeleteUnder
#else
#define MAYBE_DeleteUnder DeleteUnder
#endif
TEST(WaitableEventWatcherTest, MAYBE_DeleteUnder) {
  for (int i = 0; i < kNumTestingMessageLoops; i++) {
    RunTest_DeleteUnder(testing_message_loops[i]);
  }
}

}  // namespace base
