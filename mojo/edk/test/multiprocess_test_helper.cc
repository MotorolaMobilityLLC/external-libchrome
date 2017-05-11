// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/test/multiprocess_test_helper.h"

#include <functional>
#include <set>
#include <utility>

#include "base/base_paths.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/incoming_broker_client_invitation.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/outgoing_broker_client_invitation.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#elif defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mach_port_broker.h"
#endif

namespace mojo {
namespace edk {
namespace test {

namespace {

const char kNamedPipeName[] = "named-pipe-name";
const char kRunAsBrokerClient[] = "run-as-broker-client";

const char kTestChildMessagePipeName[] = "test_pipe";

template <typename Func>
int RunClientFunction(Func handler, bool pass_pipe_ownership_to_main) {
  CHECK(MultiprocessTestHelper::primordial_pipe.is_valid());
  ScopedMessagePipeHandle pipe =
      std::move(MultiprocessTestHelper::primordial_pipe);
  MessagePipeHandle pipe_handle =
      pass_pipe_ownership_to_main ? pipe.release() : pipe.get();
  return handler(pipe_handle.value());
}

}  // namespace

MultiprocessTestHelper::MultiprocessTestHelper() {}

MultiprocessTestHelper::~MultiprocessTestHelper() {
  CHECK(!test_child_.process.IsValid());
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChild(
    const std::string& test_child_name,
    LaunchType launch_type) {
  return StartChildWithExtraSwitch(test_child_name, std::string(),
                                   std::string(), launch_type);
}

ScopedMessagePipeHandle MultiprocessTestHelper::StartChildWithExtraSwitch(
    const std::string& test_child_name,
    const std::string& switch_string,
    const std::string& switch_value,
    LaunchType launch_type) {
  CHECK(!test_child_name.empty());
  CHECK(!test_child_.process.IsValid());

  std::string test_child_main = test_child_name + "TestChildMain";

  // Manually construct the new child's commandline to avoid copying unwanted
  // values.
  base::CommandLine command_line(
      base::GetMultiProcessTestChildBaseCommandLine().GetProgram());

  std::set<std::string> uninherited_args;
  uninherited_args.insert("mojo-platform-channel-handle");
  uninherited_args.insert(switches::kTestChildProcess);

  // Copy commandline switches from the parent process, except for the
  // multiprocess client name and mojo message pipe handle; this allows test
  // clients to spawn other test clients.
  for (const auto& entry :
          base::CommandLine::ForCurrentProcess()->GetSwitches()) {
    if (uninherited_args.find(entry.first) == uninherited_args.end())
      command_line.AppendSwitchNative(entry.first, entry.second);
  }

  PlatformChannelPair channel;
  NamedPlatformHandle named_pipe;
  HandlePassingInformation handle_passing_info;
  if (launch_type == LaunchType::CHILD || launch_type == LaunchType::PEER) {
    channel.PrepareToPassClientHandleToChildProcess(&command_line,
                                                    &handle_passing_info);
  } else if (launch_type == LaunchType::NAMED_CHILD ||
             launch_type == LaunchType::NAMED_PEER) {
#if defined(OS_POSIX)
    base::FilePath temp_dir;
    CHECK(base::PathService::Get(base::DIR_TEMP, &temp_dir));
    named_pipe = NamedPlatformHandle(
        temp_dir.AppendASCII(GenerateRandomToken()).value());
#else
    named_pipe = NamedPlatformHandle(GenerateRandomToken());
#endif
    command_line.AppendSwitchNative(kNamedPipeName, named_pipe.name);
  }

  if (!switch_string.empty()) {
    CHECK(!command_line.HasSwitch(switch_string));
    if (!switch_value.empty())
      command_line.AppendSwitchASCII(switch_string, switch_value);
    else
      command_line.AppendSwitch(switch_string);
  }

  base::LaunchOptions options;
#if defined(OS_POSIX)
  options.fds_to_remap = &handle_passing_info;
#elif defined(OS_WIN)
  options.start_hidden = true;
  if (base::win::GetVersion() >= base::win::VERSION_VISTA)
    options.handles_to_inherit = &handle_passing_info;
  else
    options.inherit_handles = true;
#else
#error "Not supported yet."
#endif

  // NOTE: In the case of named pipes, it's important that the server handle be
  // created before the child process is launched; otherwise the server binding
  // the pipe path can race with child's connection to the pipe.
  ScopedPlatformHandle server_handle;
  if (launch_type == LaunchType::CHILD || launch_type == LaunchType::PEER) {
    server_handle = channel.PassServerHandle();
  } else if (launch_type == LaunchType::NAMED_CHILD ||
             launch_type == LaunchType::NAMED_PEER) {
    server_handle = CreateServerHandle(named_pipe);
  }

  OutgoingBrokerClientInvitation child_invitation;
  ScopedMessagePipeHandle pipe;
  if (launch_type == LaunchType::CHILD ||
      launch_type == LaunchType::NAMED_CHILD) {
    pipe = child_invitation.AttachMessagePipe(kTestChildMessagePipeName);
    command_line.AppendSwitch(kRunAsBrokerClient);
  } else if (launch_type == LaunchType::PEER ||
             launch_type == LaunchType::NAMED_PEER) {
    peer_token_ = mojo::edk::GenerateRandomToken();
    pipe = ConnectToPeerProcess(std::move(server_handle), peer_token_);
  }

  test_child_ =
      base::SpawnMultiProcessTestChild(test_child_main, command_line, options);
  if (launch_type == LaunchType::CHILD || launch_type == LaunchType::PEER)
    channel.ChildProcessLaunched();

  if (launch_type == LaunchType::CHILD ||
      launch_type == LaunchType::NAMED_CHILD) {
    DCHECK(server_handle.is_valid());
    child_invitation.Send(
        test_child_.process.Handle(),
        ConnectionParams(TransportProtocol::kLegacy, std::move(server_handle)),
        process_error_callback_);
  }

  CHECK(test_child_.process.IsValid());
  return pipe;
}

int MultiprocessTestHelper::WaitForChildShutdown() {
  CHECK(test_child_.process.IsValid());

  int rv = -1;
  WaitForMultiprocessTestChildExit(test_child_.process,
                                   TestTimeouts::action_timeout(), &rv);
  test_child_.process.Close();
  return rv;
}

void MultiprocessTestHelper::ClosePeerConnection() {
  DCHECK(!peer_token_.empty());
  ::mojo::edk::ClosePeerConnection(peer_token_);
  peer_token_.clear();
}

bool MultiprocessTestHelper::WaitForChildTestShutdown() {
  return WaitForChildShutdown() == 0;
}

// static
void MultiprocessTestHelper::ChildSetup() {
  CHECK(base::CommandLine::InitializedForCurrentProcess());

  auto& command_line = *base::CommandLine::ForCurrentProcess();
  NamedPlatformHandle named_pipe(
      command_line.GetSwitchValueNative(kNamedPipeName));
  if (command_line.HasSwitch(kRunAsBrokerClient)) {
    IncomingBrokerClientInvitation invitation;
#if defined(OS_MACOSX) && !defined(OS_IOS)
    CHECK(base::MachPortBroker::ChildSendTaskPortToParent("mojo_test"));
#endif
    if (named_pipe.is_valid()) {
      invitation.Accept(ConnectionParams(TransportProtocol::kLegacy,
                                         CreateClientHandle(named_pipe)));
    } else {
      invitation.AcceptFromCommandLine(TransportProtocol::kLegacy);
    }
    primordial_pipe = invitation.ExtractMessagePipe(kTestChildMessagePipeName);
  } else {
    if (named_pipe.is_valid()) {
      primordial_pipe = ConnectToPeerProcess(CreateClientHandle(named_pipe));
    } else {
      primordial_pipe = ConnectToPeerProcess(
          PlatformChannelPair::PassClientHandleFromParentProcess(
              *base::CommandLine::ForCurrentProcess()));
    }
  }
}

// static
int MultiprocessTestHelper::RunClientMain(
    const base::Callback<int(MojoHandle)>& main,
    bool pass_pipe_ownership_to_main) {
  return RunClientFunction(
      [main](MojoHandle handle) { return main.Run(handle); },
      pass_pipe_ownership_to_main);
}

// static
int MultiprocessTestHelper::RunClientTestMain(
    const base::Callback<void(MojoHandle)>& main) {
  return RunClientFunction(
      [main](MojoHandle handle) {
        main.Run(handle);
        return (::testing::Test::HasFatalFailure() ||
                ::testing::Test::HasNonfatalFailure())
                   ? 1
                   : 0;
      },
      true /* close_pipe_on_exit */);
}

// static
mojo::ScopedMessagePipeHandle MultiprocessTestHelper::primordial_pipe;

}  // namespace test
}  // namespace edk
}  // namespace mojo
