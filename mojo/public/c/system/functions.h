// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains basic functions common to different Mojo system APIs.
//
// Note: This header should be compilable as C.

#ifndef MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
#define MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_

// Note: This header should be compilable as C.

#include "mojo/public/c/system/system_export.h"
#include "mojo/public/c/system/types.h"

// Note: Pointer parameters that are labelled "optional" may be null (at least
// under some circumstances). Non-const pointer parameters are also labeled
// "in", "out", or "in/out", to indicate how they are used. (Note that how/if
// such a parameter is used may depend on other parameters or the requested
// operation's success/failure. E.g., a separate |flags| parameter may control
// whether a given "in/out" parameter is used for input, output, or both.)

#ifdef __cplusplus
extern "C" {
#endif

// Platform-dependent monotonically increasing tick count representing "right
// now." The resolution of this clock is ~1-15ms.  Resolution varies depending
// on hardware/operating system configuration.
MOJO_SYSTEM_EXPORT MojoTimeTicks MojoGetTimeTicksNow();

// Closes the given |handle|.
//
// Returns:
//   |MOJO_RESULT_OK| on success.
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle.
//
// Concurrent operations on |handle| may succeed (or fail as usual) if they
// happen before the close, be cancelled with result |MOJO_RESULT_CANCELLED| if
// they properly overlap (this is likely the case with |MojoWait()|, etc.), or
// fail with |MOJO_RESULT_INVALID_ARGUMENT| if they happen after.
MOJO_SYSTEM_EXPORT MojoResult MojoClose(MojoHandle handle);

// Waits on the given handle until the state indicated by |flags| is satisfied
// or until |deadline| has passed.
//
// Returns:
//   |MOJO_RESULT_OK| if some flag in |flags| was satisfied (or is already
//        satisfied).
//   |MOJO_RESULT_INVALID_ARGUMENT| if |handle| is not a valid handle (e.g., if
//       it has already been closed).
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       the flags being satisfied.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that any
//       flag in |flags| will ever be satisfied.
//
// If there are multiple waiters (on different threads, obviously) waiting on
// the same handle and flag and that flag becomes set, all waiters will be
// awoken.
MOJO_SYSTEM_EXPORT MojoResult MojoWait(MojoHandle handle,
                                       MojoWaitFlags flags,
                                       MojoDeadline deadline);

// Waits on |handles[0]|, ..., |handles[num_handles-1]| for at least one of them
// to satisfy the state indicated by |flags[0]|, ..., |flags[num_handles-1]|,
// respectively, or until |deadline| has passed.
//
// Returns:
//   The index |i| (from 0 to |num_handles-1|) if |handle[i]| satisfies
//       |flags[i]|.
//   |MOJO_RESULT_INVALID_ARGUMENT| if some |handle[i]| is not a valid handle
//       (e.g., if it has already been closed).
//   |MOJO_RESULT_DEADLINE_EXCEEDED| if the deadline has passed without any of
//       handles satisfying any of its flags.
//   |MOJO_RESULT_FAILED_PRECONDITION| if it is or becomes impossible that SOME
//       |handle[i]| will ever satisfy any of its flags |flags[i]|.
MOJO_SYSTEM_EXPORT MojoResult MojoWaitMany(const MojoHandle* handles,
                                           const MojoWaitFlags* flags,
                                           uint32_t num_handles,
                                           MojoDeadline deadline);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // MOJO_PUBLIC_C_SYSTEM_FUNCTIONS_H_
