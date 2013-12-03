// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_TASK_RUNNERS_H_
#define MOJO_SHELL_TASK_RUNNERS_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/threading/thread.h"

namespace mojo {
namespace shell {

// A context object that contains the common task runners for the shell
class TaskRunners {
 public:
  explicit TaskRunners(base::SingleThreadTaskRunner* ui_runner);
  ~TaskRunners();

  base::SingleThreadTaskRunner* ui_runner() const {
    return ui_runner_.get();
  }

  base::SingleThreadTaskRunner* io_runner() const {
    return io_thread_->message_loop_proxy();
  }

  base::SingleThreadTaskRunner* file_runner() const {
    return file_thread_->message_loop_proxy();
  }

  base::MessageLoopProxy* cache_runner() const {
    return cache_thread_->message_loop_proxy();
  }

 private:
  // TODO(beng): should this be named shell_runner_?
  scoped_refptr<base::SingleThreadTaskRunner> ui_runner_;
  scoped_ptr<base::Thread> cache_thread_;
  scoped_ptr<base::Thread> io_thread_;
  scoped_ptr<base::Thread> file_thread_;

  DISALLOW_COPY_AND_ASSIGN(TaskRunners);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_TASK_RUNNERS_H_
