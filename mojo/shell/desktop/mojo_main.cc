// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "mojo/shell/child_process.h"
#include "mojo/shell/context.h"
#include "mojo/shell/init.h"
#include "mojo/shell/switches.h"

#if defined(COMPONENT_BUILD)
#include "ui/gl/gl_surface.h"
#endif

namespace {

#if defined(OS_LINUX)
// Copied from ui/gfx/switches.cc to avoid a dependency on //ui/gfx
const char kEnableHarfBuzzRenderText[] = "enable-harfbuzz-rendertext";
#endif

#if defined(OS_WIN)
void SplitString(const base::string16& str, std::vector<std::string>* argv) {
  base::SplitString(base::UTF16ToUTF8(str), ' ', argv);
}
#elif defined(OS_POSIX)
void SplitString(const std::string& str, std::vector<std::string>* argv) {
  base::SplitString(str, ' ', argv);
}
#endif

// The value of app_url_and_args is "<mojo_app_url> [<args>...]", where args
// is a list of "configuration" arguments separated by spaces. If one or more
// arguments are specified they will be available when the Mojo application
// is initialized. See ApplicationImpl::args().
GURL GetAppURLAndSetArgs(const base::CommandLine::StringType& app_url_and_args,
                         mojo::shell::Context* context) {
  std::vector<std::string> argv;
  SplitString(app_url_and_args, &argv);
  if (argv.empty())
    return GURL::EmptyGURL();
  GURL app_url(argv[0]);
  if (argv.size() > 1)
    context->application_manager()->SetArgsForURL(argv, app_url);
  return app_url;
}

void RunApps(mojo::shell::Context* context) {
  const auto& command_line = *base::CommandLine::ForCurrentProcess();
  for (const auto& arg : command_line.GetArgs())
    context->Run(GetAppURLAndSetArgs(arg, context));
}

}  // namespace

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  base::CommandLine::Init(argc, argv);
#if defined(OS_LINUX)
  // We use gfx::RenderText from multiple threads concurrently and the pango
  // backend (currently the default on linux) is not close to threadsafe. Force
  // use of the harfbuzz backend for now.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      kEnableHarfBuzzRenderText);
#endif
  mojo::shell::InitializeLogging();

  // TODO(vtl): Unify parent and child process cases to the extent possible.
  if (scoped_ptr<mojo::shell::ChildProcess> child_process =
          mojo::shell::ChildProcess::Create(
              *base::CommandLine::ForCurrentProcess())) {
    child_process->Main();
  } else {
#if defined(COMPONENT_BUILD)
    gfx::GLSurface::InitializeOneOff();
#endif
    // We want the shell::Context to outlive the MessageLoop so that pipes are
    // all gracefully closed / error-out before we try to shut the Context down.
    mojo::shell::Context shell_context;
    {
      base::MessageLoop message_loop;
      shell_context.Init();

      const base::CommandLine& command_line =
          *base::CommandLine::ForCurrentProcess();
      if (command_line.HasSwitch(switches::kOrigin)) {
        shell_context.mojo_url_resolver()->SetBaseURL(
            GURL(command_line.GetSwitchValueASCII(switches::kOrigin)));
      }

      for (const auto& kv : command_line.GetSwitches()) {
        if (kv.first == switches::kArgsFor)
          GetAppURLAndSetArgs(kv.second, &shell_context);
      }

      message_loop.PostTask(FROM_HERE, base::Bind(RunApps, &shell_context));
      message_loop.Run();
    }
  }
  return 0;
}
