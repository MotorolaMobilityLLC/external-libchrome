// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a cross platform interface for helper functions related to debuggers.
// You should use this to test if you're running under a debugger, and if you
// would like to yield (breakpoint) into the debugger.

#ifndef BASE_DEBUG_UTIL_H_
#define BASE_DEBUG_UTIL_H_

#include "base/basictypes.h"

#include <vector>

// A stacktrace can be helpful in debugging. For example, you can include a
// stacktrace member in a object (probably around #ifndef NDEBUG) so that you
// can later see where the given object was created from.
class StackTrace {
 public:
  // Create a stacktrace from the current location
  StackTrace();
  // Get an array of instruction pointer values.
  //   count: (output) the number of elements in the returned array
  const void *const *Addresses(size_t* count);
  // Print a backtrace to stderr
  void PrintBacktrace();

 private:
  std::vector<void*> trace_;

  DISALLOW_EVIL_CONSTRUCTORS(StackTrace);
};

class DebugUtil {
 public:
  // Starts the registered system-wide JIT debugger to attach it to specified
  // process.
  static bool SpawnDebuggerOnProcess(unsigned process_id);

  // Waits wait_seconds seconds for a debugger to attach to the current process.
  // When silent is false, an exception is thrown when a debugger is detected.
  static bool WaitForDebugger(int wait_seconds, bool silent);

  // Are we running under a debugger?
  // On OS X, the underlying mechanism doesn't work when the sandbox is enabled.
  // To get around this, this function caches it's value.
  // WARNING: Because of this, on OS X, a call MUST be made to this function
  // BEFORE the sandbox is enabled.
  static bool BeingDebugged();

  // Break into the debugger, assumes a debugger is present.
  static void BreakDebugger();
};

#endif  // BASE_DEBUG_UTIL_H_
