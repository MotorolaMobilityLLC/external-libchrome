// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/run.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/shell/context.h"
#include "mojo/shell/keep_alive.h"
#include "mojo/shell/switches.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

void Run(Context* context) {
  KeepAlive keep_alive(context);

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  base::CommandLine::StringVector args = command_line.GetArgs();

  if (args.empty()) {
    LOG(ERROR) << "No app path specified.";
    return;
  }

  for (base::CommandLine::StringVector::const_iterator it = args.begin();
       it != args.end(); ++it) {
    GURL url(*it);
    if (url.scheme() == "mojo" && !command_line.HasSwitch(switches::kOrigin)) {
      LOG(ERROR) << "mojo: url passed with no --origin specified.";
      return;
    }
    ScopedMessagePipeHandle no_handle;
    context->service_manager()->ConnectToService(
        GURL(*it), std::string(), no_handle.Pass());
  }
}

}  // namespace shell
}  // namespace mojo
