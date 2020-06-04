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

#include "base/base_paths.h"

#include "base/file_util.h"
#include "base/path_service.h"

namespace base {

bool PathProvider(int key, std::wstring* result) {
  // NOTE: DIR_CURRENT is a special cased in PathService::Get

  std::wstring cur;
  switch (key) {
    case base::DIR_EXE:
      PathService::Get(base::FILE_EXE, &cur);
      file_util::TrimFilename(&cur);
      break;
    case base::DIR_MODULE:
      PathService::Get(base::FILE_MODULE, &cur);
      file_util::TrimFilename(&cur);
      break;
    case base::DIR_TEMP:
      if (!file_util::GetTempDir(&cur))
        return false;
      break;
    case base::DIR_SOURCE_ROOT:
      // By default, unit tests execute two levels deep from the source root.
      // For example:  chrome/{Debug|Release}/ui_tests.exe
      PathProvider(base::DIR_EXE, &cur);
      file_util::UpOneDirectory(&cur);
      file_util::UpOneDirectory(&cur);
      break;
    default:
      return false;
  }

  result->swap(cur);
  return true;
}

}  // namespace base
