// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/launcher/test_launcher.h"

#if defined(OS_POSIX)
#include <fcntl.h>
#endif

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/process/kill.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/launcher/test_results_tracker.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace base {

// See https://groups.google.com/a/chromium.org/d/msg/chromium-dev/nkdTP7sstSc/uT3FaE_sgkAJ .
using ::operator<<;

// The environment variable name for the total number of test shards.
const char kTestTotalShards[] = "GTEST_TOTAL_SHARDS";
// The environment variable name for the test shard index.
const char kTestShardIndex[] = "GTEST_SHARD_INDEX";

namespace {

// Set of live launch test processes with corresponding lock (it is allowed
// for callers to launch processes on different threads).
LazyInstance<std::set<ProcessHandle> > g_live_process_handles
    = LAZY_INSTANCE_INITIALIZER;
LazyInstance<Lock> g_live_process_handles_lock = LAZY_INSTANCE_INITIALIZER;

#if defined(OS_POSIX)
// Self-pipe that makes it possible to do complex shutdown handling
// outside of the signal handler.
int g_shutdown_pipe[2] = { -1, -1 };

void ShutdownPipeSignalHandler(int signal) {
  HANDLE_EINTR(write(g_shutdown_pipe[1], "q", 1));
}

// I/O watcher for the reading end of the self-pipe above.
// Terminates any launched child processes and exits the process.
class SignalFDWatcher : public MessageLoopForIO::Watcher {
 public:
  SignalFDWatcher() {
  }

  virtual void OnFileCanReadWithoutBlocking(int fd) OVERRIDE {
    fprintf(stdout, "\nCaught signal. Killing spawned test processes...\n");
    fflush(stdout);

    // Keep the lock until exiting the process to prevent further processes
    // from being spawned.
    AutoLock lock(g_live_process_handles_lock.Get());

    fprintf(stdout,
            "Sending SIGTERM to %" PRIuS " child processes... ",
            g_live_process_handles.Get().size());
    fflush(stdout);

    for (std::set<ProcessHandle>::iterator i =
             g_live_process_handles.Get().begin();
         i != g_live_process_handles.Get().end();
         ++i) {
      kill((-1) * (*i), SIGTERM);  // Send the signal to entire process group.
    }

    fprintf(stdout,
            "done.\nGiving processes a chance to terminate cleanly... ");
    fflush(stdout);

    PlatformThread::Sleep(TimeDelta::FromMilliseconds(500));

    fprintf(stdout, "done.\n");
    fflush(stdout);

    fprintf(stdout,
            "Sending SIGKILL to %" PRIuS " child processes... ",
            g_live_process_handles.Get().size());
    fflush(stdout);

    for (std::set<ProcessHandle>::iterator i =
             g_live_process_handles.Get().begin();
         i != g_live_process_handles.Get().end();
         ++i) {
      kill((-1) * (*i), SIGKILL);  // Send the signal to entire process group.
    }

    fprintf(stdout, "done.\n");
    fflush(stdout);

    // The signal would normally kill the process, so exit now.
    exit(1);
  }

  virtual void OnFileCanWriteWithoutBlocking(int fd) OVERRIDE {
    NOTREACHED();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SignalFDWatcher);
};
#endif  // defined(OS_POSIX)

// Parses the environment variable var as an Int32.  If it is unset, returns
// default_val.  If it is set, unsets it then converts it to Int32 before
// returning it.  If unsetting or converting to an Int32 fails, print an
// error and exit with failure.
int32 Int32FromEnvOrDie(const char* const var, int32 default_val) {
  scoped_ptr<Environment> env(Environment::Create());
  std::string str_val;
  int32 result;
  if (!env->GetVar(var, &str_val))
    return default_val;
  if (!env->UnSetVar(var)) {
    LOG(ERROR) << "Invalid environment: we could not unset " << var << ".\n";
    exit(EXIT_FAILURE);
  }
  if (!StringToInt(str_val, &result)) {
    LOG(ERROR) << "Invalid environment: " << var << " is not an integer.\n";
    exit(EXIT_FAILURE);
  }
  return result;
}

// Checks whether sharding is enabled by examining the relevant
// environment variable values.  If the variables are present,
// but inconsistent (i.e., shard_index >= total_shards), prints
// an error and exits.
void InitSharding(int32* total_shards, int32* shard_index) {
  *total_shards = Int32FromEnvOrDie(kTestTotalShards, 1);
  *shard_index = Int32FromEnvOrDie(kTestShardIndex, 0);

  if (*total_shards == -1 && *shard_index != -1) {
    LOG(ERROR) << "Invalid environment variables: you have "
               << kTestShardIndex << " = " << *shard_index
               << ", but have left " << kTestTotalShards << " unset.\n";
    exit(EXIT_FAILURE);
  } else if (*total_shards != -1 && *shard_index == -1) {
    LOG(ERROR) << "Invalid environment variables: you have "
               << kTestTotalShards << " = " << *total_shards
               << ", but have left " << kTestShardIndex << " unset.\n";
    exit(EXIT_FAILURE);
  } else if (*shard_index < 0 || *shard_index >= *total_shards) {
    LOG(ERROR) << "Invalid environment variables: we require 0 <= "
               << kTestShardIndex << " < " << kTestTotalShards
               << ", but you have " << kTestShardIndex << "=" << *shard_index
               << ", " << kTestTotalShards << "=" << *total_shards << ".\n";
    exit(EXIT_FAILURE);
  }
}

// Given the total number of shards, the shard index, and the test id, returns
// true iff the test should be run on this shard.  The test id is some arbitrary
// but unique non-negative integer assigned by this launcher to each test
// method.  Assumes that 0 <= shard_index < total_shards, which is first
// verified in ShouldShard().
bool ShouldRunTestOnShard(int total_shards, int shard_index, int test_id) {
  return (test_id % total_shards) == shard_index;
}

// For a basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc, see the comment below and http://crbug.com/44497)
bool PatternMatchesString(const char* pattern, const char* str) {
  switch (*pattern) {
    case '\0':
    case ':':  // Either ':' or '\0' marks the end of the pattern.
      return *str == '\0';
    case '?':  // Matches any single character.
      return *str != '\0' && PatternMatchesString(pattern + 1, str + 1);
    case '*':  // Matches any string (possibly empty) of characters.
      return (*str != '\0' && PatternMatchesString(pattern, str + 1)) ||
          PatternMatchesString(pattern + 1, str);
    default:  // Non-special character.  Matches itself.
      return *pattern == *str &&
          PatternMatchesString(pattern + 1, str + 1);
  }
}

// TODO(phajdan.jr): Avoid duplicating gtest code. (http://crbug.com/44497)
// For basic pattern matching for gtest_filter options.  (Copied from
// gtest.cc)
bool MatchesFilter(const std::string& name, const std::string& filter) {
  const char *cur_pattern = filter.c_str();
  for (;;) {
    if (PatternMatchesString(cur_pattern, name.c_str())) {
      return true;
    }

    // Finds the next pattern in the filter.
    cur_pattern = strchr(cur_pattern, ':');

    // Returns if no more pattern can be found.
    if (cur_pattern == NULL) {
      return false;
    }

    // Skips the pattern separater (the ':' character).
    cur_pattern++;
  }
}

void RunTests(TestLauncherDelegate* launcher_delegate,
              int total_shards,
              int shard_index,
              const ResultsPrinter::TestsResultCallback& callback) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();

  DCHECK(!command_line->HasSwitch(kGTestListTestsFlag));

  testing::UnitTest* const unit_test = testing::UnitTest::GetInstance();

  std::string filter = command_line->GetSwitchValueASCII(kGTestFilterFlag);

  // Split --gtest_filter at '-', if there is one, to separate into
  // positive filter and negative filter portions.
  std::string positive_filter = filter;
  std::string negative_filter;
  size_t dash_pos = filter.find('-');
  if (dash_pos != std::string::npos) {
    positive_filter = filter.substr(0, dash_pos);  // Everything up to the dash.
    negative_filter = filter.substr(dash_pos + 1); // Everything after the dash.
  }

  int num_runnable_tests = 0;

  // ResultsPrinter detects when all tests are done and deletes itself.
  ResultsPrinter* printer = new ResultsPrinter(*command_line, callback);

  for (int i = 0; i < unit_test->total_test_case_count(); ++i) {
    const testing::TestCase* test_case = unit_test->GetTestCase(i);
    for (int j = 0; j < test_case->total_test_count(); ++j) {
      const testing::TestInfo* test_info = test_case->GetTestInfo(j);
      std::string test_name = test_info->test_case_name();
      test_name.append(".");
      test_name.append(test_info->name());

      // Skip disabled tests.
      if (test_name.find("DISABLED") != std::string::npos &&
          !command_line->HasSwitch(kGTestRunDisabledTestsFlag)) {
        continue;
      }

      std::string filtering_test_name =
          launcher_delegate->GetTestNameForFiltering(test_case, test_info);

      // Skip the test that doesn't match the filter string (if given).
      if ((!positive_filter.empty() &&
           !MatchesFilter(filtering_test_name, positive_filter)) ||
          MatchesFilter(filtering_test_name, negative_filter)) {
        continue;
      }

      if (!launcher_delegate->ShouldRunTest(test_case, test_info))
        continue;

      bool should_run = ShouldRunTestOnShard(total_shards, shard_index,
                                             num_runnable_tests);
      num_runnable_tests++;
      if (!should_run)
        continue;

      printer->OnTestStarted(test_name);
      MessageLoop::current()->PostTask(
          FROM_HERE,
          Bind(&TestLauncherDelegate::RunTest,
               Unretained(launcher_delegate),
               test_case,
               test_info,
               Bind(&ResultsPrinter::AddTestResult, Unretained(printer))));
    }
  }

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&TestLauncherDelegate::RunRemainingTests,
      Unretained(launcher_delegate)));

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&ResultsPrinter::OnAllTestsStarted, printer->GetWeakPtr()));
}

void RunTestIteration(TestLauncherDelegate* launcher_delegate,
                      int32 total_shards,
                      int32 shard_index,
                      int cycles,
                      int* exit_code,
                      bool run_tests_success) {
  if (!run_tests_success) {
    *exit_code = 1;
    MessageLoop::current()->Quit();
    return;
  }

  if (cycles == 0) {
    MessageLoop::current()->Quit();
    return;
  }

  // Special value "-1" means "repeat indefinitely".
  int new_cycles = (cycles == -1) ? cycles : cycles - 1;

  launcher_delegate->OnTestIterationStarting();

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&RunTests, launcher_delegate, total_shards, shard_index,
           Bind(&RunTestIteration,
                launcher_delegate,
                total_shards,
                shard_index,
                new_cycles,
                exit_code)));
}


}  // namespace

const char kGTestFilterFlag[] = "gtest_filter";
const char kGTestHelpFlag[]   = "gtest_help";
const char kGTestListTestsFlag[] = "gtest_list_tests";
const char kGTestRepeatFlag[] = "gtest_repeat";
const char kGTestRunDisabledTestsFlag[] = "gtest_also_run_disabled_tests";
const char kGTestOutputFlag[] = "gtest_output";

TestResult::TestResult() : status(TEST_UNKNOWN) {
}

TestLauncherDelegate::~TestLauncherDelegate() {
}

void PrintTestOutputSnippetOnFailure(const TestResult& result,
                                     const std::string& full_output) {
  if (result.status == TestResult::TEST_SUCCESS)
    return;

  size_t run_pos = full_output.find(std::string("[ RUN      ] ") +
                                    result.GetFullName());
  if (run_pos == std::string::npos)
    return;

  size_t end_pos = full_output.find(std::string("[  FAILED  ] ") +
                                    result.GetFullName(),
                                    run_pos);
  if (end_pos != std::string::npos) {
    size_t newline_pos = full_output.find("\n", end_pos);
    if (newline_pos != std::string::npos)
      end_pos = newline_pos + 1;
  }

  std::string snippet(full_output.substr(run_pos));
  if (end_pos != std::string::npos)
    snippet = full_output.substr(run_pos, end_pos - run_pos);

  // TODO(phajdan.jr): Indent each line of the snippet so it's more
  // noticeable.
  fprintf(stdout, "%s", snippet.c_str());
  fflush(stdout);
}

int LaunchChildGTestProcess(const CommandLine& command_line,
                            const std::string& wrapper,
                            base::TimeDelta timeout,
                            bool* was_timeout) {
  LaunchOptions options;

#if defined(OS_POSIX)
  // On POSIX, we launch the test in a new process group with pgid equal to
  // its pid. Any child processes that the test may create will inherit the
  // same pgid. This way, if the test is abruptly terminated, we can clean up
  // any orphaned child processes it may have left behind.
  options.new_process_group = true;
#endif

  return LaunchChildTestProcessWithOptions(
      PrepareCommandLineForGTest(command_line, wrapper),
      options,
      timeout,
      was_timeout);
}

CommandLine PrepareCommandLineForGTest(const CommandLine& command_line,
                                       const std::string& wrapper) {
  CommandLine new_command_line(command_line.GetProgram());
  CommandLine::SwitchMap switches = command_line.GetSwitches();

  // Strip out gtest_repeat flag - this is handled by the launcher process.
  switches.erase(kGTestRepeatFlag);

  for (CommandLine::SwitchMap::const_iterator iter = switches.begin();
       iter != switches.end(); ++iter) {
    new_command_line.AppendSwitchNative((*iter).first, (*iter).second);
  }

  // Prepend wrapper after last CommandLine quasi-copy operation. CommandLine
  // does not really support removing switches well, and trying to do that
  // on a CommandLine with a wrapper is known to break.
  // TODO(phajdan.jr): Give it a try to support CommandLine removing switches.
#if defined(OS_WIN)
  new_command_line.PrependWrapper(ASCIIToWide(wrapper));
#elif defined(OS_POSIX)
  new_command_line.PrependWrapper(wrapper);
#endif

  return new_command_line;
}

int LaunchChildTestProcessWithOptions(const CommandLine& command_line,
                                      const LaunchOptions& options,
                                      base::TimeDelta timeout,
                                      bool* was_timeout) {
#if defined(OS_POSIX)
  // Make sure an option we rely on is present - see LaunchChildGTestProcess.
  DCHECK(options.new_process_group);
#endif

  LaunchOptions new_options(options);

#if defined(OS_WIN)
  DCHECK(!new_options.job_handle);

  win::ScopedHandle job_handle(CreateJobObject(NULL, NULL));
  if (!job_handle.IsValid()) {
    LOG(ERROR) << "Could not create JobObject.";
    return -1;
  }

  // Allow break-away from job since sandbox and few other places rely on it
  // on Windows versions prior to Windows 8 (which supports nested jobs).
  // TODO(phajdan.jr): Do not allow break-away on Windows 8.
  if (!SetJobObjectLimitFlags(job_handle.Get(),
                              JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                              JOB_OBJECT_LIMIT_BREAKAWAY_OK)) {
    LOG(ERROR) << "Could not SetJobObjectLimitFlags.";
    return -1;
  }

  new_options.job_handle = job_handle.Get();
#endif  // defined(OS_WIN)

  base::ProcessHandle process_handle;

  {
    // Note how we grab the lock before the process possibly gets created.
    // This ensures that when the lock is held, ALL the processes are registered
    // in the set.
    AutoLock lock(g_live_process_handles_lock.Get());

    if (!base::LaunchProcess(command_line, new_options, &process_handle))
      return -1;

    g_live_process_handles.Get().insert(process_handle);
  }

  int exit_code = 0;
  if (!base::WaitForExitCodeWithTimeout(process_handle,
                                        &exit_code,
                                        timeout)) {
    *was_timeout = true;
    exit_code = -1;  // Set a non-zero exit code to signal a failure.

    // Ensure that the process terminates.
    base::KillProcess(process_handle, -1, true);
  }

  {
    // Note how we grab the log before issuing a possibly broad process kill.
    // Other code parts that grab the log kill processes, so avoid trying
    // to do that twice and trigger all kinds of log messages.
    AutoLock lock(g_live_process_handles_lock.Get());

#if defined(OS_POSIX)
    if (exit_code != 0) {
      // On POSIX, in case the test does not exit cleanly, either due to a crash
      // or due to it timing out, we need to clean up any child processes that
      // it might have created. On Windows, child processes are automatically
      // cleaned up using JobObjects.
      base::KillProcessGroup(process_handle);
    }
#endif

    g_live_process_handles.Get().erase(process_handle);
  }

  base::CloseProcessHandle(process_handle);

  return exit_code;
}

int LaunchTests(TestLauncherDelegate* launcher_delegate,
                int argc,
                char** argv) {
  const CommandLine* command_line = CommandLine::ForCurrentProcess();


  int32 total_shards;
  int32 shard_index;
  InitSharding(&total_shards, &shard_index);

  int cycles = 1;
  if (command_line->HasSwitch(kGTestRepeatFlag))
    StringToInt(command_line->GetSwitchValueASCII(kGTestRepeatFlag), &cycles);

  int exit_code = 0;

#if defined(OS_POSIX)
  CHECK_EQ(0, pipe(g_shutdown_pipe));

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  sigemptyset(&action.sa_mask);
  action.sa_handler = &ShutdownPipeSignalHandler;

  CHECK_EQ(0, sigaction(SIGINT, &action, NULL));
  CHECK_EQ(0, sigaction(SIGQUIT, &action, NULL));
  CHECK_EQ(0, sigaction(SIGTERM, &action, NULL));

  MessageLoopForIO::FileDescriptorWatcher controller;
  SignalFDWatcher watcher;

  CHECK(MessageLoopForIO::current()->WatchFileDescriptor(
            g_shutdown_pipe[0],
            true,
            MessageLoopForIO::WATCH_READ,
            &controller,
            &watcher));
#endif  // defined(OS_POSIX)

  MessageLoop::current()->PostTask(
      FROM_HERE,
      Bind(&RunTestIteration,
           launcher_delegate,
           total_shards,
           shard_index,
           cycles,
           &exit_code,
           true));

  MessageLoop::current()->Run();

  return exit_code;
}

}  // namespace base
