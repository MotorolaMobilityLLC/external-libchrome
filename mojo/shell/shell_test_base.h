// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SHELL_TEST_BASE_H_
#define MOJO_SHELL_SHELL_TEST_BASE_H_

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/shell/context.h"
#include "testing/gtest/include/gtest/gtest.h"

class GURL;

namespace mojo {
namespace shell {
namespace test {

class ShellTestBase : public testing::Test {
 public:
  ShellTestBase();
  virtual ~ShellTestBase();

  // Launches the given service in-process; |service_url| should typically be a
  // mojo: URL (the origin will be set to an "appropriate" file: URL).
  void LaunchServiceInProcess(const GURL& service_url,
                              const std::string& service_name,
                              ScopedMessagePipeHandle client_handle);

  base::MessageLoop* message_loop() { return &message_loop_; }
  Context* shell_context() { return &shell_context_; }

 private:
  base::MessageLoop message_loop_;
  Context shell_context_;

  DISALLOW_COPY_AND_ASSIGN(ShellTestBase);
};

}  // namespace test
}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SHELL_TEST_BASE_H_
