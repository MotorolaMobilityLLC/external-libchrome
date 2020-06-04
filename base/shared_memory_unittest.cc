// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/platform_thread.h"
#include "base/shared_memory.h"
#include "testing/gtest/include/gtest/gtest.h"

static const int kNumThreads = 5;

namespace {

class SharedMemoryTest : public testing::Test {
};

// Each thread will open the shared memory.  Each thread will take a different 4
// byte int pointer, and keep changing it, with some small pauses in between.
// Verify that each thread's value in the shared memory is always correct.
class MultipleThreadMain : public PlatformThread::Delegate {
 public:
  explicit MultipleThreadMain(int16 id) : id_(id) {}
  ~MultipleThreadMain() {}

  // PlatformThread::Delegate interface.
  void ThreadMain() {
    const int kDataSize = 1024;
    std::wstring test_name = L"SharedMemoryOpenThreadTest";
    SharedMemory memory;
    bool rv = memory.Create(test_name, false, true, kDataSize);
    EXPECT_TRUE(rv);
    rv = memory.Map(kDataSize);
    EXPECT_TRUE(rv);
    int *ptr = static_cast<int*>(memory.memory()) + id_;
    EXPECT_EQ(*ptr, 0);

    for (int idx = 0; idx < 100; idx++) {
      *ptr = idx;
      PlatformThread::Sleep(1);  // Short wait.
      EXPECT_EQ(*ptr, idx);
    }

    memory.Close();
  }

 private:
  int16 id_;

  DISALLOW_COPY_AND_ASSIGN(MultipleThreadMain);
};

#if defined(OS_WIN)
// Each thread will open the shared memory.  Each thread will take the memory,
// and keep changing it while trying to lock it, with some small pauses in
// between. Verify that each thread's value in the shared memory is always
// correct.
class MultipleLockThread : public PlatformThread::Delegate {
 public:
  explicit MultipleLockThread(int id) : id_(id) {}
  ~MultipleLockThread() {}

  // PlatformThread::Delegate interface.
  void ThreadMain() {
    const int kDataSize = sizeof(int);
    SharedMemoryHandle handle = NULL;
    {
      SharedMemory memory1;
      EXPECT_TRUE(memory1.Create(L"SharedMemoryMultipleLockThreadTest",
                                 false, true, kDataSize));
      EXPECT_TRUE(memory1.ShareToProcess(GetCurrentProcess(), &handle));
      // TODO(paulg): Implement this once we have a posix version of
      // SharedMemory::ShareToProcess.
      EXPECT_TRUE(true);
    }

    SharedMemory memory2(handle, false);
    EXPECT_TRUE(memory2.Map(kDataSize));
    volatile int* const ptr = static_cast<int*>(memory2.memory());

    for (int idx = 0; idx < 20; idx++) {
      memory2.Lock();
      int i = (id_ << 16) + idx;
      *ptr = i;
      PlatformThread::Sleep(1);  // Short wait.
      EXPECT_EQ(*ptr, i);
      memory2.Unlock();
    }

    memory2.Close();
  }

 private:
  int id_;

  DISALLOW_COPY_AND_ASSIGN(MultipleLockThread);
};
#endif

}  // namespace

TEST(SharedMemoryTest, OpenClose) {
  const int kDataSize = 1024;
  std::wstring test_name = L"SharedMemoryOpenCloseTest";

  // Open two handles to a memory segment, confirm that they are mapped
  // separately yet point to the same space.
  SharedMemory memory1;
  bool rv = memory1.Open(test_name, false);
  EXPECT_FALSE(rv);
  rv = memory1.Create(test_name, false, false, kDataSize);
  EXPECT_TRUE(rv);
  rv = memory1.Map(kDataSize);
  EXPECT_TRUE(rv);
  SharedMemory memory2;
  rv = memory2.Open(test_name, false);
  EXPECT_TRUE(rv);
  rv = memory2.Map(kDataSize);
  EXPECT_TRUE(rv);
  EXPECT_NE(memory1.memory(), memory2.memory());  // Compare the pointers.

  // Make sure we don't segfault. (it actually happened!)
  ASSERT_NE(memory1.memory(), static_cast<void*>(NULL));
  ASSERT_NE(memory2.memory(), static_cast<void*>(NULL));

  // Write data to the first memory segment, verify contents of second.
  memset(memory1.memory(), '1', kDataSize);
  EXPECT_EQ(memcmp(memory1.memory(), memory2.memory(), kDataSize), 0);

  // Close the first memory segment, and verify the second has the right data.
  memory1.Close();
  char *start_ptr = static_cast<char *>(memory2.memory());
  char *end_ptr = start_ptr + kDataSize;
  for (char* ptr = start_ptr; ptr < end_ptr; ptr++)
    EXPECT_EQ(*ptr, '1');

  // Close the second memory segment.
  memory2.Close();
}

#if defined(OS_WIN)
// Create a set of 5 threads to each open a shared memory segment and write to
// it. Verify that they are always reading/writing consistent data.
TEST(SharedMemoryTest, MultipleThreads) {
  PlatformThreadHandle thread_handles[kNumThreads];
  MultipleThreadMain* thread_delegates[kNumThreads];

  // Spawn the threads.
  for (int16 index = 0; index < kNumThreads; index++) {
    PlatformThreadHandle pth;
    thread_delegates[index] = new MultipleThreadMain(index);
    EXPECT_TRUE(PlatformThread::Create(0, thread_delegates[index], &pth));
    thread_handles[index] = pth;
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; index++) {
    PlatformThread::Join(thread_handles[index]);
    delete thread_delegates[index];
  }
}

// Create a set of threads to each open a shared memory segment and write to it
// with the lock held. Verify that they are always reading/writing consistent
// data.
TEST(SharedMemoryTest, Lock) {
  PlatformThreadHandle thread_handles[kNumThreads];
  MultipleLockThread* thread_delegates[kNumThreads];

  // Spawn the threads.
  for (int index = 0; index < kNumThreads; ++index) {
    PlatformThreadHandle pth;
    thread_delegates[index] = new MultipleLockThread(index);
    EXPECT_TRUE(PlatformThread::Create(0, thread_delegates[index], &pth));
    thread_handles[index] = pth;
  }

  // Wait for the threads to finish.
  for (int index = 0; index < kNumThreads; ++index) {
    PlatformThread::Join(thread_handles[index]);
    delete thread_delegates[index];
  }
}
#endif
