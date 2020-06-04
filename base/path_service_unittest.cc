// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest/include/gtest/gtest-spi.h"

namespace {

// Returns true if PathService::Get returns true and sets the path parameter
// to non-empty for the given PathService::DirType enumeration value.
bool ReturnsValidPath(int dir_type) {
  std::wstring path;
  bool result = PathService::Get(dir_type, &path);
  return result && !path.empty() && file_util::PathExists(path);
}

// Function to test DIR_LOCAL_APP_DATA_LOW on Windows XP. Make sure it fails.
bool ReturnsInvalidPath(int dir_type) {
  std::wstring path;
  bool result = PathService::Get(base::DIR_LOCAL_APP_DATA_LOW, &path);
  return !result && path.empty();
}

}  // namespace

// Test that all PathService::Get calls return a value and a true result
// in the development environment.  (This test was created because a few
// later changes to Get broke the semantics of the function and yielded the
// correct value while returning false.)
TEST(PathServiceTest, Get) {
  for (int key = base::DIR_CURRENT; key < base::PATH_END; ++key) {
    EXPECT_PRED1(ReturnsValidPath, key);
  }
#ifdef OS_WIN
  for (int key = base::PATH_WIN_START + 1; key < base::PATH_WIN_END; ++key) {
    if (key == base::DIR_LOCAL_APP_DATA_LOW &&
        win_util::GetWinVersion() < win_util::WINVERSION_VISTA) {
      // DIR_LOCAL_APP_DATA_LOW is not supported prior Vista and is expected to
      // fail.
      EXPECT_TRUE(ReturnsInvalidPath(key)) << key;
    } else {
      EXPECT_TRUE(ReturnsValidPath(key)) << key;
    }
  }
#endif
}
