// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PLATFORM_THREAD_H_
#define BASE_PLATFORM_THREAD_H_

#include "base/basictypes.h"

// PlatformThreadHandle should not be assumed to be a numeric type, since the
// standard intends to allow pthread_t to be a structure.  This means you
// should not initialize it to a value, like 0.  If it's a member variable, the
// constructor can safely "value initialize" using () in the initializer list.
#if defined(OS_WIN)
typedef void* PlatformThreadHandle;  // HANDLE
#elif defined(OS_POSIX)
#include <pthread.h>
typedef pthread_t PlatformThreadHandle;
#endif

// A namespace for low-level thread functions.
class PlatformThread {
 public:
  // Gets the current thread id, which may be useful for logging purposes.
  static int CurrentId();

  // Yield the current thread so another thread can be scheduled.
  static void YieldCurrentThread();

  // Sleeps for the specified duration (units are milliseconds).
  static void Sleep(int duration_ms);

  // Sets the thread name visible to a debugger.  This has no effect otherwise.
  // To set the name of the current thread, pass PlatformThread::CurrentId() as
  // the thread_id parameter.
  static void SetName(int thread_id, const char* name);

  // Implement this interface to run code on a background thread.  Your
  // ThreadMain method will be called on the newly created thread.
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void ThreadMain() = 0;
  };

  // Creates a new thread.  The |stack_size| parameter can be 0 to indicate
  // that the default stack size should be used.  Upon success,
  // |*thread_handle| will be assigned a handle to the newly created thread,
  // and |delegate|'s ThreadMain method will be executed on the newly created
  // thread.
  // NOTE: When you are done with the thread handle, you must call Join to
  // release system resources associated with the thread.  You must ensure that
  // the Delegate object outlives the thread.
  static bool Create(size_t stack_size, Delegate* delegate,
                     PlatformThreadHandle* thread_handle);

  // Joins with a thread created via the Create function.  This function blocks
  // the caller until the designated thread exits.  This will invalidate
  // |thread_handle|.
  static void Join(PlatformThreadHandle thread_handle);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PlatformThread);
};

#endif  // BASE_PLATFORM_THREAD_H_

