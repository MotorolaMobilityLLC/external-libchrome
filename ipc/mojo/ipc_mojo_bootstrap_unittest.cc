// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_mojo_bootstrap.h"

#include <stdint.h>

#include "base/base_paths.h"
#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_test_base.h"
#include "ipc/mojo/ipc.mojom.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/test/mojo_test_base.h"
#include "mojo/edk/test/multiprocess_test_helper.h"
#include "mojo/edk/test/scoped_ipc_support.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace {

class IPCMojoBootstrapTest : public mojo::edk::test::MojoTestBase {
 protected:
};

class TestingDelegate : public IPC::MojoBootstrap::Delegate {
 public:
  explicit TestingDelegate(const base::Closure& quit_callback)
      : passed_(false), quit_callback_(quit_callback) {}

  void OnPipesAvailable(IPC::mojom::ChannelAssociatedPtrInfo send_channel,
                        IPC::mojom::ChannelAssociatedRequest receive_channel,
                        int32_t peer_pid) override;
  void OnBootstrapError() override;

  bool passed() const { return passed_; }

 private:
  bool passed_;
  const base::Closure quit_callback_;
};

void TestingDelegate::OnPipesAvailable(
    IPC::mojom::ChannelAssociatedPtrInfo send_channel,
    IPC::mojom::ChannelAssociatedRequest receive_channel,
    int32_t peer_pid) {
  passed_ = true;
  quit_callback_.Run();
}

void TestingDelegate::OnBootstrapError() {
  quit_callback_.Run();
}

// Times out on Android; see http://crbug.com/502290
#if defined(OS_ANDROID)
#define MAYBE_Connect DISABLED_Connect
#else
#define MAYBE_Connect Connect
#endif
TEST_F(IPCMojoBootstrapTest, MAYBE_Connect) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  TestingDelegate delegate(run_loop.QuitClosure());
  RUN_CHILD_ON_PIPE(IPCMojoBootstrapTestClient, pipe)
  scoped_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(pipe)),
      IPC::Channel::MODE_SERVER, &delegate);

  bootstrap->Connect();
  run_loop.Run();

  EXPECT_TRUE(delegate.passed());
  END_CHILD()
}

class IPCMojoBootstrapTestClient {

};

}  // namespace

namespace mojo {
namespace edk {
namespace {

// A long running process that connects to us.
DEFINE_TEST_CLIENT_TEST_WITH_PIPE(IPCMojoBootstrapTestClient,
                                  IPCMojoBootstrapTest,
                                  pipe) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  TestingDelegate delegate(run_loop.QuitClosure());
  scoped_ptr<IPC::MojoBootstrap> bootstrap = IPC::MojoBootstrap::Create(
      mojo::MakeScopedHandle(mojo::MessagePipeHandle(pipe)),
      IPC::Channel::MODE_CLIENT, &delegate);

  bootstrap->Connect();

  run_loop.Run();

  EXPECT_TRUE(delegate.passed());
}

}  // namespace
}  // namespace edk
}  // namespace mojo
