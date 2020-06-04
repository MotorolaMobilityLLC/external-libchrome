// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <iostream>

#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/synchronization/waitable_event.h"
#include "base/trace_event/trace_event.h"
#include "components/tracing/trace_config_file.h"
#include "components/tracing/tracing_switches.h"
#include "mojo/runner/context.h"
#include "mojo/runner/switches.h"
#include "mojo/runner/tracer.h"

namespace mojo {
namespace runner {

int LauncherProcessMain(int argc, char** argv) {
  mojo::runner::Tracer tracer;
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  bool trace_startup = command_line.HasSwitch(switches::kTraceStartup);
  if (trace_startup) {
    tracer.Start(
        command_line.GetSwitchValueASCII(switches::kTraceStartup),
        command_line.GetSwitchValueASCII(switches::kTraceStartupDuration),
        "mandoline.trace");
  }

  // We want the shell::Context to outlive the MessageLoop so that pipes are
  // all gracefully closed / error-out before we try to shut the Context down.
  base::FilePath shell_dir;
  PathService::Get(base::DIR_MODULE, &shell_dir);
  Context shell_context(shell_dir, &tracer);
  {
    base::MessageLoop message_loop;
    tracer.DidCreateMessageLoop();
    if (!shell_context.Init()) {
      return 0;
    }

    message_loop.PostTask(
        FROM_HERE,
        base::Bind(&Context::RunCommandLineApplication,
                   base::Unretained(&shell_context), base::Closure()));
    message_loop.Run();

    // Must be called before |message_loop| is destroyed.
    shell_context.Shutdown();
  }

  return 0;
}

}  // namespace runner
}  // namespace mojo
