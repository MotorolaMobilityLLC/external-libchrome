// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_switches.h"

namespace switches {

// If the program includes chrome/common/debug_on_start.h, the process will
// start the JIT system-registered debugger on itself and will wait for 60
// seconds for the debugger to attach to itself. Then a break point will be hit.
const wchar_t kDebugOnStart[]                  = L"debug-on-start";

// Will wait for 60 seconds for a debugger to come to attach to the process.
const wchar_t kWaitForDebugger[]               = L"wait-for-debugger";

// Suppresses all error dialogs when present.
const wchar_t kNoErrorDialogs[]                = L"noerrdialogs";

// Disables the crash reporting.
const wchar_t kDisableBreakpad[]               = L"disable-breakpad";

// Generates full memory crash dump.
const wchar_t kFullMemoryCrashReport[]         = L"full-memory-crash-report";

// The value of this switch determines whether the process is started as a
// renderer or plugin host.  If it's empty, it's the browser.
const wchar_t kProcessType[]                   = L"type";

// Enable DCHECKs in release mode.
const wchar_t kEnableDCHECK[]                  = L"enable-dcheck";

// Disable win_util::MessageBox.  This is useful when running as part of
// scripts that do not have a user interface.
const wchar_t kNoMessageBox[]                  = L"no-message-box";

}  // namespace switches
