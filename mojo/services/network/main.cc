// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/network_service_impl.h"

class Delegate : public mojo::ApplicationDelegate {
 public:
  Delegate() {}

  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
    base::FilePath base_path;
    CHECK(PathService::Get(base::DIR_TEMP, &base_path));
    base_path = base_path.Append(FILE_PATH_LITERAL("network_service"));
    context_.reset(new mojo::NetworkContext(base_path));
  }

  virtual bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) MOJO_OVERRIDE {
    DCHECK(context_);
    connection->AddService<mojo::NetworkServiceImpl>(context_.get());
    return true;
  }

 private:
  scoped_ptr<mojo::NetworkContext> context_;
};

extern "C" APPLICATION_EXPORT MojoResult CDECL MojoMain(
    MojoHandle shell_handle) {
  base::CommandLine::Init(0, NULL);
#if !defined(COMPONENT_BUILD)
  base::AtExitManager at_exit;
#endif

  // The IO message loop allows us to use net::URLRequest on this thread.
  base::MessageLoopForIO loop;

  Delegate delegate;
  mojo::ApplicationImpl app(
      &delegate, mojo::MakeScopedHandle(mojo::MessagePipeHandle(shell_handle)));

  loop.Run();
  return MOJO_RESULT_OK;
}
