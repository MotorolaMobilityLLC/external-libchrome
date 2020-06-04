// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace logging {

namespace {

using ::testing::Return;

class LoggingTest : public testing::Test {
};

class MockLogSource {
 public:
  MOCK_METHOD0(Log, const char*());
};

TEST_F(LoggingTest, BasicLogging) {
  MockLogSource mock_log_source;
  const int kExpectedDebugOrReleaseCalls = 6;
  const int kExpectedDebugCalls = 6;
  const int kExpectedCalls =
      kExpectedDebugOrReleaseCalls + (DEBUG_MODE ? kExpectedDebugCalls : 0);
  EXPECT_CALL(mock_log_source, Log()).Times(kExpectedCalls).
      WillRepeatedly(Return("log message"));

  SetMinLogLevel(LOG_INFO);

  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_EQ(DEBUG_MODE != 0, DLOG_IS_ON(INFO));
  EXPECT_TRUE(VLOG_IS_ON(0));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, true) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, true) << mock_log_source.Log();
  VLOG(0) << mock_log_source.Log();
  VLOG_IF(0, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(0) << mock_log_source.Log();
  DVLOG_IF(0, true) << mock_log_source.Log();
}

TEST_F(LoggingTest, LoggingIsLazy) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  SetMinLogLevel(LOG_WARNING);

  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(DLOG_IS_ON(INFO));
  EXPECT_FALSE(VLOG_IS_ON(1));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, false) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, false) << mock_log_source.Log();
  VLOG(1) << mock_log_source.Log();
  VLOG_IF(1, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(1) << mock_log_source.Log();
  DVLOG_IF(1, true) << mock_log_source.Log();
}

TEST_F(LoggingTest, ChecksAreLazy) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  SetMinLogLevel(LOG_FATAL + 1);
  EXPECT_FALSE(LOG_IS_ON(FATAL));

  CHECK(mock_log_source.Log());
  PCHECK(mock_log_source.Log());
  CHECK_EQ(mock_log_source.Log(), static_cast<const char*>(NULL))
      << mock_log_source.Log();
}

TEST_F(LoggingTest, DchecksAreLazy) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

#if defined(NDEBUG)
  logging::g_enable_dcheck = false;
#else
  SetMinLogLevel(LOG_FATAL + 1);
  EXPECT_FALSE(LOG_IS_ON(FATAL));
#endif
  DCHECK(mock_log_source.Log());
  DPCHECK(mock_log_source.Log());
  DCHECK_EQ(0, 0) << mock_log_source.Log();
  DCHECK_EQ(mock_log_source.Log(), static_cast<const char*>(NULL))
      << mock_log_source.Log();
}

}  // namespace

}  // namespace logging
