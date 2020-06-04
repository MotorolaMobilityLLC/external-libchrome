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

#ifndef BASE_BASE_PATHS_H__
#define BASE_BASE_PATHS_H__

// This file declares path keys for the base module.  These can be used with
// the PathService to access various special directories and files.

namespace base {

enum {
  PATH_START = 0,

  DIR_CURRENT,  // current directory
  DIR_EXE,      // directory containing FILE_EXE
  DIR_MODULE,   // directory containing FILE_MODULE
  FILE_EXE,     // path and filename of the current executable
  FILE_MODULE,  // path and filename of the module containing the code for the
                // PathService (which could differ from FILE_EXE if the
                // PathService were compiled into a DLL, for example)
  DIR_TEMP,     // temporary directory
  DIR_WINDOWS,  // Windows directory, usually "c:\windows"
  DIR_SYSTEM,   // Usually c:\windows\system32"
  DIR_PROGRAM_FILES, // Usually c:\program files

  DIR_SOURCE_ROOT,  // Returns the root of the source tree.  This key is useful
                    // for tests that need to locate various resources.  It
                    // should not be used outside of test code.
  DIR_APP_DATA,  // Application Data directory under the user profile.
  DIR_LOCAL_APP_DATA_LOW,  // Local AppData directory for low integrity level.
  DIR_LOCAL_APP_DATA,  // "Local Settings\Application Data" directory under the
                       // user profile.
  DIR_IE_INTERNET_CACHE,  // Temporary Internet Files directory.
  DIR_COMMON_START_MENU,  // Usually "C:\Documents and Settings\All Users\
                          // Start Menu\Programs"
  DIR_START_MENU,         // Usually "C:\Documents and Settings\<user>\
                          // Start Menu\Programs"
  PATH_END
};

}  // namespace base

#endif  // BASE_BASE_PATHS_H__
