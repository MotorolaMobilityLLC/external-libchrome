// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"  // TODO(vtl): Remove.
#include "base/message_loop/message_loop.h"
#include "mojo/common/message_pump_mojo.h"  // TODO(vtl): Remove.
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/shell/child_process.h"
#include "mojo/shell/child_process_host.h"  // TODO(vtl): Remove.
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"
#include "mojo/shell/run.h"
#include "mojo/shell/switches.h"
#include "ui/gl/gl_surface.h"

namespace {

// TODO(vtl): Remove.
class TestChildProcessHostDelegate
    : public mojo::shell::ChildProcessHost::Delegate {
 public:
  TestChildProcessHostDelegate() {}
  virtual ~TestChildProcessHostDelegate() {}
  virtual void WillStart() OVERRIDE {
    VLOG(2) << "TestChildProcessHostDelegate::WillStart()";
  }
  virtual void DidStart(bool success) OVERRIDE {
    VLOG(2) << "TestChildProcessHostDelegate::DidStart(" << success << ")";
    base::MessageLoop::current()->QuitWhenIdle();
  }
};

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  mojo::Environment env;
  base::CommandLine::Init(argc, argv);
  mojo::shell::InitializeLogging();

  // TODO(vtl): Move this a proper test (and remove includes marked "remove").
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("run-test-child")) {
    base::MessageLoop message_loop(
        scoped_ptr<base::MessagePump>(new mojo::common::MessagePumpMojo()));

    mojo::shell::Context context;
    TestChildProcessHostDelegate child_process_host_delegate;
    mojo::shell::ChildProcessHost child_process_host(
        &context, &child_process_host_delegate,
        mojo::shell::ChildProcess::TYPE_TEST);
    child_process_host.Start();
    message_loop.Run();
    int exit_code = child_process_host.Join();
    VLOG(2) << "Joined child: exit_code = " << exit_code;
    return 0;
  }

  // TODO(vtl): Unify parent and child process cases to the extent possible.
  if (scoped_ptr<mojo::shell::ChildProcess> child_process =
          mojo::shell::ChildProcess::Create(
              *base::CommandLine::ForCurrentProcess())) {
    child_process->Main();
  } else {
    gfx::GLSurface::InitializeOneOff();

    base::MessageLoop message_loop;
    mojo::shell::Context shell_context;

    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    if (command_line.HasSwitch(switches::kOrigin)) {
      shell_context.set_mojo_origin(
          command_line.GetSwitchValueASCII(switches::kOrigin));
    }

    std::vector<GURL> app_urls;
    base::CommandLine::StringVector args = command_line.GetArgs();
    for (base::CommandLine::StringVector::const_iterator it = args.begin();
         it != args.end();
         ++it)
      app_urls.push_back(GURL(*it));

    message_loop.PostTask(FROM_HERE,
                          base::Bind(mojo::shell::Run,
                                     &shell_context,
                                     app_urls));

    message_loop.Run();
  }

  return 0;
}
