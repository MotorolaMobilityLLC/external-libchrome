// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_BACKGROUND_BACKGROUND_SHELL_H_
#define MOJO_SHELL_BACKGROUND_BACKGROUND_SHELL_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"

class GURL;

namespace mojo {
namespace shell {

struct CommandLineSwitch;

// BackgroundShell starts up the mojo shell on a background thread, and
// destroys the thread in the destructor. Once created use CreateApplication()
// to obtain an InterfaceRequest for the Application. The InterfaceRequest can
// then be bound to an ApplicationImpl.
class BackgroundShell {
 public:
  BackgroundShell();
  ~BackgroundShell();

  // Starts the background shell. |command_line_switches| are additional
  // switches applied to any processes spawned by this call.
  void Init(const std::vector<CommandLineSwitch>& command_line_switches);

  // Obtains an InterfaceRequest for the specified url.
  InterfaceRequest<mojom::ShellClient> CreateShellClientRequest(
      const GURL& url);

 private:
  class MojoThread;

  scoped_ptr<MojoThread> thread_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundShell);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_BACKGROUND_BACKGROUND_SHELL_H_
