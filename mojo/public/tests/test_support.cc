// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/tests/test_support.h"

#include "base/test/perf_log.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {
namespace test {

bool WriteTextMessage(MessagePipeHandle handle, const std::string& text) {
  MojoResult rv = WriteMessageRaw(handle,
                                  text.data(),
                                  static_cast<uint32_t>(text.size()),
                                  NULL,
                                  0,
                                  MOJO_WRITE_MESSAGE_FLAG_NONE);
  return rv == MOJO_RESULT_OK;
}

bool ReadTextMessage(MessagePipeHandle handle, std::string* text) {
  MojoResult rv;
  bool did_wait = false;

  uint32_t num_bytes = 0, num_handles = 0;
  for (;;) {
    rv = ReadMessageRaw(handle,
                        NULL,
                        &num_bytes,
                        NULL,
                        &num_handles,
                        MOJO_READ_MESSAGE_FLAG_NONE);
    if (rv == MOJO_RESULT_SHOULD_WAIT) {
      if (did_wait) {
        assert(false);  // Looping endlessly!?
        return false;
      }
      rv = Wait(handle, MOJO_WAIT_FLAG_READABLE, MOJO_DEADLINE_INDEFINITE);
      if (rv != MOJO_RESULT_OK)
        return false;
      did_wait = true;
    } else {
      assert(!num_handles);
      break;
    }
  }

  text->resize(num_bytes);
  rv = ReadMessageRaw(handle,
                      &text->at(0),
                      &num_bytes,
                      NULL,
                      &num_handles,
                      MOJO_READ_MESSAGE_FLAG_NONE);
  return rv == MOJO_RESULT_OK;
}

void IterateAndReportPerf(const char* test_name,
                          PerfTestSingleIteration single_iteration,
                          void* closure) {
  // TODO(vtl): These should be specifiable using command-line flags.
  static const size_t kGranularity = 100;
  static const MojoTimeTicks kPerftestTimeMicroseconds = 3 * 1000000;

  const MojoTimeTicks start_time = GetTimeTicksNow();
  MojoTimeTicks end_time;
  size_t iterations = 0;
  do {
    for (size_t i = 0; i < kGranularity; i++)
      (*single_iteration)(closure);
    iterations += kGranularity;

    end_time = GetTimeTicksNow();
  } while (end_time - start_time < kPerftestTimeMicroseconds);

  base::LogPerfResult(test_name,
                      1000000.0 * iterations / (end_time - start_time),
                      "iterations/second");
}

MojoResult WriteEmptyMessage(const MessagePipeHandle& handle) {
  return WriteMessageRaw(handle, NULL, 0, NULL, 0,
                         MOJO_WRITE_MESSAGE_FLAG_NONE);
}

MojoResult ReadEmptyMessage(const MessagePipeHandle& handle) {
  return ReadMessageRaw(handle, NULL, NULL, NULL, NULL,
                        MOJO_READ_MESSAGE_FLAG_MAY_DISCARD);
}

}  // namespace test
}  // namespace mojo
