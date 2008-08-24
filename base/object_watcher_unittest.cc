// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <process.h>

#include "base/message_loop.h"
#include "base/object_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
typedef testing::Test ObjectWatcherTest;

class QuitDelegate : public base::ObjectWatcher::Delegate {
 public:
  virtual void OnObjectSignaled(HANDLE object) {
    MessageLoop::current()->Quit();
  }
};

class DecrementCountDelegate : public base::ObjectWatcher::Delegate {
 public:
  DecrementCountDelegate(int* counter) : counter_(counter) {
  }
  virtual void OnObjectSignaled(HANDLE object) {
    --(*counter_);
  }
 private:
  int* counter_;
};

}

TEST(ObjectWatcherTest, BasicSignal) {
  base::ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);
  
  SetEvent(event);

  MessageLoop::current()->Run();

  CloseHandle(event);
}

TEST(ObjectWatcherTest, BasicCancel) {
  base::ObjectWatcher watcher;

  // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  QuitDelegate delegate;
  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);
  
  watcher.StopWatching();

  CloseHandle(event);
}


TEST(ObjectWatcherTest, CancelAfterSet) {
  base::ObjectWatcher watcher;

  int counter = 1;
  DecrementCountDelegate delegate(&counter);

    // A manual-reset event that is not yet signaled.
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);

  bool ok = watcher.StartWatching(event, &delegate);
  EXPECT_TRUE(ok);
   
  SetEvent(event);
    
  // Let the background thread do its business
  Sleep(30);
    
  watcher.StopWatching();
  
  MessageLoop::current()->RunAllPending();

  // Our delegate should not have fired.
  EXPECT_EQ(1, counter);
  
  CloseHandle(event);
}

// Used so we can simulate a MessageLoop that dies before an ObjectWatcher.
// This ordinarily doesn't happen when people use the Thread class, but it can
// happen when people use the Singleton pattern or atexit.
static unsigned __stdcall ThreadFunc(void* param) {
  HANDLE event = CreateEvent(NULL, TRUE, FALSE, NULL);  // not signaled
  {
    base::ObjectWatcher watcher;
    {
      MessageLoop message_loop;

      QuitDelegate delegate;
      watcher.StartWatching(event, &delegate);
    }
  }
  CloseHandle(event);
  return 0;
}

TEST(ObjectWatcherTest, OutlivesMessageLoop) {
  unsigned int thread_id;
  HANDLE thread = reinterpret_cast<HANDLE>(
      _beginthreadex(NULL,
                     0,
                     ThreadFunc,
                     NULL,
                     0,
                     &thread_id));
  WaitForSingleObject(thread, INFINITE);
  CloseHandle(thread);
}

