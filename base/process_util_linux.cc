// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process_util.h"

#include <ctype.h>
#include <dirent.h>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/time.h"

namespace {

enum ParsingState {
  KEY_NAME,
  KEY_VALUE
};

}  // namespace

namespace base {

bool LaunchApp(const std::vector<std::string>& argv,
               bool wait, ProcessHandle* process_handle) {
  bool retval = true;

  char* argv_copy[argv.size() + 1];
  for (size_t i = 0; i < argv.size(); i++) {
    argv_copy[i] = new char[argv[i].size() + 1];
    strcpy(argv_copy[i], argv[i].c_str());
  }
  argv_copy[argv.size()] = NULL;

  int pid = fork();
  if (pid == 0) {
    execvp(argv_copy[0], argv_copy);
  } else if (pid < 0) {
    retval = false;
  } else {
    if (wait)
      waitpid(pid, 0, 0);

    if(process_handle)
      *process_handle = pid;
  }

  for (size_t i = 0; i < argv.size(); i++)
    delete[] argv_copy[i];

  return retval;
}

bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle) {
  return LaunchApp(cl.argv(), wait, process_handle);
}

// Attempts to kill the process identified by the given process
// entry structure.  Ignores specified exit_code; linux can't force that.
// Returns true if this is successful, false otherwise.
bool KillProcess(int process_id, int exit_code, bool wait) {
  bool result = false;

  int status = kill(process_id, SIGTERM);
  if (!status && wait) {
    int tries = 60;
    // The process may not end immediately due to pending I/O
    while (tries-- > 0) {
      int pid = waitpid(process_id, &status, WNOHANG);
      if (pid == process_id) {
        result = true;
        break;
      }
      sleep(1);
    }
  }
  if (!result)
    DLOG(ERROR) << "Unable to terminate process.";
  return result;
}

bool DidProcessCrash(ProcessHandle handle) {
  int status;
  if (waitpid(handle, &status, WNOHANG)) {
    // I feel like dancing!
    return false;
  }

  if (WIFSIGNALED(status)) {
    int signum = WTERMSIG(status);
    return (signum == SIGSEGV || signum == SIGILL || signum == SIGABRT || signum == SIGFPE);
  }

  if (WIFEXITED(status)) {
    int exitcode = WEXITSTATUS(status);
    return (exitcode != 0);
  }

  return false;
}

NamedProcessIterator::NamedProcessIterator(const std::wstring& executable_name,
                                           const ProcessFilter* filter) 
    :
       executable_name_(executable_name),
       filter_(filter) {
    procfs_dir_ = opendir("/proc");
  }

NamedProcessIterator::~NamedProcessIterator() {
  if (procfs_dir_) {
    closedir(procfs_dir_);
    procfs_dir_ = 0;
  }
}

const ProcessEntry* NamedProcessIterator::NextProcessEntry() {
  bool result = false;
  do {
    result = CheckForNextProcess();
  } while (result && !IncludeEntry());

  if (result)
    return &entry_;

  return NULL;
}

bool NamedProcessIterator::CheckForNextProcess() {
  // TODO(port): skip processes owned by different UID

  dirent* slot = 0;
  const char* openparen;
  const char* closeparen;

  // Arbitrarily guess that there will never be more than 200 non-process files in /proc.
  // (Hardy has 53.)
  int skipped = 0;
  const int kSkipLimit = 200;
  while (skipped < kSkipLimit) {
    slot = readdir(procfs_dir_);
    // all done looking through /proc?
    if (!slot)
      return false;

    // If not a process, keep looking for one.
    bool notprocess = false;
    int i;
    for (i=0; i < NAME_MAX && slot->d_name[i]; ++i) {
       if (!isdigit(slot->d_name[i])) {
         notprocess = true;
         break;
       }
    }
    if (i == NAME_MAX || notprocess) {
      skipped++;
      continue;
    }

    // Read the process's status.
    char buf[NAME_MAX + 12];
    sprintf(buf, "/proc/%s/stat", slot->d_name);
    FILE *fp = fopen(buf, "r");
    if (!fp)
      return false;
    const char* result = fgets(buf, sizeof(buf), fp);
    fclose(fp);
    if (!result)
      return false;

    // Parse the status.  It is formatted like this:
    // %d (%s) %c %d ...
    // pid (name) runstate ppid
    // To avoid being fooled by names containing a closing paren, scan backwards.
    openparen = strchr(buf, '(');
    closeparen = strrchr(buf, ')');
    if (!openparen || !closeparen)
      return false;
    char runstate = closeparen[2];

    // Is the process in 'Zombie' state, i.e. dead but waiting to be reaped?
    // Allowed values: D R S T Z
    if (runstate != 'Z')
      break;

    // Nope, it's a zombie; somebody isn't cleaning up after their children.
    // (e.g. WaitForProcessesToExit doesn't clean up after dead children yet.)
    // There could be a lot of zombies, can't really decrement i here.
  }
  if (skipped >= kSkipLimit) {
    NOTREACHED();
    return false;
  }

  entry_.pid = atoi(slot->d_name);
  entry_.ppid = atoi(closeparen+3);

  // TODO(port): read pid's commandline's $0, like killall does.
  // Using the short name between openparen and closeparen won't work for long names!
  int len = closeparen - openparen - 1;
  if (len > NAME_MAX)
    len = NAME_MAX;
  memcpy(entry_.szExeFile, openparen + 1, len);
  entry_.szExeFile[len] = 0;

  return true;
}

bool NamedProcessIterator::IncludeEntry() {
  // TODO(port): make this also work for non-ASCII filenames
  bool result = strcmp(WideToASCII(executable_name_).c_str(), entry_.szExeFile) == 0 &&
      (!filter_ || filter_->Includes(entry_.pid, entry_.ppid));
  return result;
}

int GetProcessCount(const std::wstring& executable_name,
                    const ProcessFilter* filter) {
  int count = 0;

  NamedProcessIterator iter(executable_name, filter);
  while (iter.NextProcessEntry())
    ++count;
  return count;
}

bool KillProcesses(const std::wstring& executable_name, int exit_code,
                   const ProcessFilter* filter) {
  bool result = true;
  const ProcessEntry* entry;

  NamedProcessIterator iter(executable_name, filter);
  while ((entry = iter.NextProcessEntry()) != NULL)
    result = KillProcess((*entry).pid, exit_code, true) && result;

  return result;
}

bool WaitForProcessesToExit(const std::wstring& executable_name,
                            int wait_milliseconds,
                            const ProcessFilter* filter) {
  bool result = false;

  // TODO(port): This is inefficient, but works if there are multiple procs.
  // TODO(port): use waitpid to avoid leaving zombies around

  base::Time end_time = base::Time::Now() + base::TimeDelta::FromMilliseconds(wait_milliseconds);
  do {
    NamedProcessIterator iter(executable_name, filter);
    if (!iter.NextProcessEntry()) {
      result = true;
      break;
    }
    // TODO(port): Improve resolution
    sleep(1);
  } while ((base::Time::Now() - end_time) > base::TimeDelta());

  return result;
}

bool WaitForSingleProcess(ProcessHandle handle, int wait_milliseconds) {
  int status;
  waitpid(handle, &status, 0);
  return WIFEXITED(status);
}

bool CleanupProcesses(const std::wstring& executable_name,
                      int wait_milliseconds,
                      int exit_code,
                      const ProcessFilter* filter) {
  bool exited_cleanly =
    WaitForProcessesToExit(executable_name, wait_milliseconds,
                           filter);
  if (!exited_cleanly)
    KillProcesses(executable_name, exit_code, filter);
  return exited_cleanly;
}

///////////////////////////////////////////////////////////////////////////////
//// ProcessMetrics

// To have /proc/self/io file you must enable CONFIG_TASK_IO_ACCOUNTING
// in your kernel configuration.
bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) {
  std::string proc_io_contents;
  if (!file_util::ReadFileToString(L"/proc/self/io", &proc_io_contents))
    return false;

  (*io_counters).OtherOperationCount = 0;
  (*io_counters).OtherTransferCount = 0;

  StringTokenizer tokenizer(proc_io_contents, ": \n");
  ParsingState state = KEY_NAME;
  std::string last_key_name;
  while (tokenizer.GetNext()) {
    switch (state) {
      case KEY_NAME:
        last_key_name = tokenizer.token();
        state = KEY_VALUE;
        break;
      case KEY_VALUE:
        DCHECK(!last_key_name.empty());
        if (last_key_name == "syscr") {
          (*io_counters).ReadOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "syscw") {
          (*io_counters).WriteOperationCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "rchar") {
          (*io_counters).ReadTransferCount = StringToInt64(tokenizer.token());
        } else if (last_key_name == "wchar") {
          (*io_counters).WriteTransferCount = StringToInt64(tokenizer.token());
        }
        state = KEY_NAME;
        break;
    }
  }
  return true;
}

}  // namespace base
