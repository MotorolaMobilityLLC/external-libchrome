// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include <sstream>
#include <string>

#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/environment/logging.h"
#include "mojo/public/cpp/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

// A macro, so it can be automatically joined with other string literals. (Not
// simply __FILE__, since that may contain a path.)
#define OUR_FILENAME "logging_unittest.cc"

namespace mojo {
namespace {

class LoggingTest : public testing::Test {
 public:
  LoggingTest() : environment_(NULL, &kMockLogger) {
    minimum_log_level_ = MOJO_LOG_LEVEL_INFO;
    ResetMockLogger();
  }
  virtual ~LoggingTest() {}

 protected:
  // Note: Does not reset |minimum_log_level_|.
  static void ResetMockLogger() {
    log_message_was_called_ = false;
    last_log_level_ = MOJO_LOG_LEVEL_INFO;
    last_message_.clear();
  }

  static bool log_message_was_called() { return log_message_was_called_; }
  static MojoLogLevel last_log_level() { return last_log_level_; }
  static const std::string& last_message() { return last_message_; }

 private:
  // Note: We record calls even if |log_level| is below |minimum_log_level_|
  // (since the macros should mostly avoid this, and we want to be able to check
  // that they do).
  static void MockLogMessage(MojoLogLevel log_level, const char* message) {
    log_message_was_called_ = true;
    last_log_level_ = log_level;
    last_message_ = message;
  }

  static MojoLogLevel MockGetMinimumLogLevel() {
    return minimum_log_level_;
  }

  static void MockSetMinimumLogLevel(MojoLogLevel minimum_log_level) {
    minimum_log_level_ = minimum_log_level;
  }

  Environment environment_;

  static const MojoLogger kMockLogger;
  static MojoLogLevel minimum_log_level_;
  static bool log_message_was_called_;
  static MojoLogLevel last_log_level_;
  static std::string last_message_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(LoggingTest);
};

// static
const MojoLogger LoggingTest::kMockLogger = {
  &LoggingTest::MockLogMessage,
  &LoggingTest::MockGetMinimumLogLevel,
  &LoggingTest::MockSetMinimumLogLevel
};

// static
MojoLogLevel LoggingTest::minimum_log_level_ = MOJO_LOG_LEVEL_INFO;

// static
bool LoggingTest::log_message_was_called_ = MOJO_LOG_LEVEL_INFO;

// static
MojoLogLevel LoggingTest::last_log_level_ = MOJO_LOG_LEVEL_INFO;

// static
std::string LoggingTest::last_message_;

std::string ExpectedLogMessage(int line, const char* message) {
  std::ostringstream s;
  s << OUR_FILENAME "(" << line << "): " << message;
  return s.str();
}

// A function returning |bool| that shouldn't be called.
bool NotCalled() {
  abort();
  // I think all compilers are smart enough to recognize that nothing is run
  // after |abort()|. (Some definitely complain that things after it are
  // unreachable.)
}

TEST_F(LoggingTest, InternalLogMessage) {
  internal::LogMessage("foo.cc", 123, MOJO_LOG_LEVEL_INFO).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ("foo.cc(123): hello world", last_message());

  ResetMockLogger();

  internal::LogMessage("./path/to/foo.cc", 123, MOJO_LOG_LEVEL_WARNING).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_WARNING, last_log_level());
  EXPECT_EQ("foo.cc(123): hello world", last_message());

  ResetMockLogger();

  internal::LogMessage("/path/to/foo.cc", 123, MOJO_LOG_LEVEL_ERROR).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_ERROR, last_log_level());
  EXPECT_EQ("foo.cc(123): hello world", last_message());

  ResetMockLogger();

  internal::LogMessage("path/to/foo.cc", 123, MOJO_LOG_LEVEL_FATAL).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_FATAL, last_log_level());
  EXPECT_EQ("foo.cc(123): hello world", last_message());

  ResetMockLogger();

  internal::LogMessage(".\\xy\\foo.cc", 123, MOJO_LOG_LEVEL_VERBOSE).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_VERBOSE, last_log_level());
  EXPECT_EQ("foo.cc(123): hello world", last_message());

  ResetMockLogger();

  internal::LogMessage("xy\\foo.cc", 123, MOJO_LOG_LEVEL_VERBOSE-1).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_VERBOSE-1, last_log_level());
  EXPECT_EQ("foo.cc(123): hello world", last_message());

  ResetMockLogger();

  internal::LogMessage("C:\\xy\\foo.cc", 123, MOJO_LOG_LEVEL_VERBOSE-9).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_VERBOSE-9, last_log_level());
  EXPECT_EQ("foo.cc(123): hello world", last_message());

  ResetMockLogger();

  internal::LogMessage(__FILE__, 123, MOJO_LOG_LEVEL_INFO).stream()
      << "hello " << "world";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ(OUR_FILENAME "(123): hello world", last_message());
}

TEST_F(LoggingTest, LogStream) {
  MOJO_LOG_STREAM(INFO) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hello"), last_message());

  ResetMockLogger();

  MOJO_LOG_STREAM(ERROR) << "hi " << 123;
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_ERROR, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hi 123"), last_message());
}

TEST_F(LoggingTest, LazyLogStream) {
  MOJO_LAZY_LOG_STREAM(INFO, true) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hello"), last_message());

  ResetMockLogger();

  MOJO_LAZY_LOG_STREAM(ERROR, true) << "hi " << 123;
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_ERROR, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hi 123"), last_message());

  ResetMockLogger();

  MOJO_LAZY_LOG_STREAM(INFO, false) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_LAZY_LOG_STREAM(FATAL, false) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  bool x = false;
  // This probably fails to compile if we forget to parenthesize the condition
  // in the macro (= has low precedence, and needs an lvalue on the LHS).
  MOJO_LAZY_LOG_STREAM(ERROR, x = true) << "hello";
  EXPECT_TRUE(log_message_was_called());

  ResetMockLogger();

  MOJO_LAZY_LOG_STREAM(WARNING, x = false) << "hello";
  EXPECT_FALSE(log_message_was_called());
}

TEST_F(LoggingTest, ShouldLog) {
  // We start at |MOJO_LOG_LEVEL_INFO|.
  EXPECT_FALSE(MOJO_SHOULD_LOG(VERBOSE));
  EXPECT_TRUE(MOJO_SHOULD_LOG(INFO));
  EXPECT_TRUE(MOJO_SHOULD_LOG(WARNING));
  EXPECT_TRUE(MOJO_SHOULD_LOG(ERROR));
  EXPECT_TRUE(MOJO_SHOULD_LOG(FATAL));

  Environment::GetDefaultLogger()->SetMinimumLogLevel(MOJO_LOG_LEVEL_ERROR);
  EXPECT_FALSE(MOJO_SHOULD_LOG(VERBOSE));
  EXPECT_FALSE(MOJO_SHOULD_LOG(INFO));
  EXPECT_FALSE(MOJO_SHOULD_LOG(WARNING));
  EXPECT_TRUE(MOJO_SHOULD_LOG(ERROR));
  EXPECT_TRUE(MOJO_SHOULD_LOG(FATAL));

  Environment::GetDefaultLogger()->SetMinimumLogLevel(MOJO_LOG_LEVEL_VERBOSE-1);
  EXPECT_TRUE(MOJO_SHOULD_LOG(VERBOSE));
  EXPECT_TRUE(MOJO_SHOULD_LOG(INFO));
  EXPECT_TRUE(MOJO_SHOULD_LOG(WARNING));
  EXPECT_TRUE(MOJO_SHOULD_LOG(ERROR));
  EXPECT_TRUE(MOJO_SHOULD_LOG(FATAL));
}

TEST_F(LoggingTest, Log) {
  // We start at |MOJO_LOG_LEVEL_INFO|.
  MOJO_LOG(VERBOSE) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_LOG(INFO) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hello"), last_message());

  ResetMockLogger();

  MOJO_LOG(ERROR) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_ERROR, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hello"), last_message());

  ResetMockLogger();

  Environment::GetDefaultLogger()->SetMinimumLogLevel(MOJO_LOG_LEVEL_ERROR);

  MOJO_LOG(VERBOSE) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_LOG(INFO) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_LOG(ERROR) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_ERROR, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hello"), last_message());
}

TEST_F(LoggingTest, LogIf) {
  // We start at |MOJO_LOG_LEVEL_INFO|.
  MOJO_LOG_IF(VERBOSE, true) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_LOG_IF(VERBOSE, false) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  bool x = false;
  // Also try to make sure that we parenthesize the condition properly.
  MOJO_LOG_IF(INFO, x = true) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hello"), last_message());

  ResetMockLogger();

  MOJO_LOG_IF(INFO, x = false) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  Environment::GetDefaultLogger()->SetMinimumLogLevel(MOJO_LOG_LEVEL_ERROR);

  ResetMockLogger();

  MOJO_LOG_IF(INFO, 0 != 1) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_LOG_IF(WARNING, 1+1 == 2) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_LOG_IF(ERROR, 1*2 == 2) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_ERROR, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-3, "hello"), last_message());

  ResetMockLogger();

  MOJO_LOG_IF(FATAL, 1*2 == 3) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  // |MOJO_LOG_IF()| shouldn't evaluate its condition if the level is below the
  // minimum.
  MOJO_LOG_IF(INFO, NotCalled()) << "hello";
  EXPECT_FALSE(log_message_was_called());
}

TEST_F(LoggingTest, Check) {
  MOJO_CHECK(true) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  bool x = true;
  // Also try to make sure that we parenthesize the condition properly.
  MOJO_CHECK(x = false) << "hello";
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_FATAL, last_log_level());
  // Different compilers have different ideas about the line number of a split
  // line.
  int line = __LINE__;
  EXPECT_EQ(ExpectedLogMessage(line-5, "Check failed: x = false. hello"),
            last_message());

  ResetMockLogger();

  // Also test a "naked" |MOJO_CHECK()|s.
  MOJO_CHECK(1+2 == 3);
  EXPECT_FALSE(log_message_was_called());
}

TEST_F(LoggingTest, Dlog) {
  // We start at |MOJO_LOG_LEVEL_INFO|.
  MOJO_DLOG(VERBOSE) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_DLOG(INFO) << "hello";
#ifdef NDEBUG
  EXPECT_FALSE(log_message_was_called());
#else
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-6, "hello"), last_message());
#endif
}

TEST_F(LoggingTest, DlogIf) {
  // We start at |MOJO_LOG_LEVEL_INFO|. It shouldn't evaluate the condition in
  // this case.
  MOJO_DLOG_IF(VERBOSE, NotCalled()) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_DLOG_IF(INFO, 1 == 0) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_DLOG_IF(INFO, 1 == 1) << "hello";
#ifdef NDEBUG
  EXPECT_FALSE(log_message_was_called());
#else
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_INFO, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-6, "hello"), last_message());
#endif

  ResetMockLogger();

  // |MOJO_DLOG_IF()| shouldn't compile its condition for non-debug builds.
#ifndef NDEBUG
  bool debug_only = true;
#endif
  MOJO_DLOG_IF(WARNING, debug_only) << "hello";
#ifdef NDEBUG
  EXPECT_FALSE(log_message_was_called());
#else
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_WARNING, last_log_level());
  EXPECT_EQ(ExpectedLogMessage(__LINE__-6, "hello"), last_message());
#endif
}

TEST_F(LoggingTest, Dcheck) {
  MOJO_DCHECK(true);
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  MOJO_DCHECK(true) << "hello";
  EXPECT_FALSE(log_message_was_called());

  ResetMockLogger();

  // |MOJO_DCHECK()| should compile (but not evaluate) its condition even for
  // non-debug builds. (Hopefully, we'll get an unused variable error if it
  // fails to compile the condition.)
  bool x = true;
  MOJO_DCHECK(x = false) << "hello";
#ifdef NDEBUG
  EXPECT_FALSE(log_message_was_called());
#else
  EXPECT_TRUE(log_message_was_called());
  EXPECT_EQ(MOJO_LOG_LEVEL_FATAL, last_log_level());
  // Different compilers have different ideas about the line number of a split
  // line.
  int line = __LINE__;
  EXPECT_EQ(ExpectedLogMessage(line-8, "Check failed: x = false. hello"),
            last_message());
#endif
}

}  // namespace
}  // namespace mojo
