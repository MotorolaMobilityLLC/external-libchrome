// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This is a cross platform interface for helper functions related to debuggers.
// You should use this to test if you're running under a debugger, and if you
// would like to yield (breakpoint) into the debugger.

#ifndef BASE_DEBUG_UTIL_H_
#define BASE_DEBUG_UTIL_H_

class DebugUtil {
 public:
  // Starts the registered system-wide JIT debugger to attach it to specified
  // process.
  static bool SpawnDebuggerOnProcess(unsigned process_id);

  // Waits wait_seconds seconds for a debugger to attach to the current process.
  // When silent is false, an exception is thrown when a debugger is detected.
  static bool WaitForDebugger(int wait_seconds, bool silent);

  // Are we running under a debugger?
  static bool BeingDebugged();

  // Break into the debugger, assumes a debugger is present.
  static void BreakDebugger();
};

#endif  // BASE_DEBUG_UTIL_H_
