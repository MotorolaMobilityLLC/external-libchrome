// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/in_process_dynamic_service_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_native_library.h"

namespace mojo {
namespace shell {

InProcessDynamicServiceRunner::InProcessDynamicServiceRunner(
    Context* /*context*/)
    : thread_(this, "app_thread") {
}

InProcessDynamicServiceRunner::~InProcessDynamicServiceRunner() {
  if (thread_.HasBeenStarted()) {
    DCHECK(!thread_.HasBeenJoined());
    thread_.Join();
  }
}

void InProcessDynamicServiceRunner::Start(
    const base::FilePath& app_path,
    ScopedShellHandle service_handle,
    const base::Closure& app_completed_callback) {
  app_path_ = app_path;

  DCHECK(!service_handle_.is_valid());
  service_handle_ = service_handle.Pass();

  DCHECK(app_completed_callback_.is_null());
  app_completed_callback_ = app_completed_callback;

  DCHECK(!thread_.HasBeenStarted());
  thread_.Start();
}

void InProcessDynamicServiceRunner::Run() {
  DVLOG(2) << "Loading/running Mojo app from " << app_path_.value()
           << " in process";

  base::ScopedClosureRunner app_deleter(
      base::Bind(base::IgnoreResult(&base::DeleteFile), app_path_, false));

  do {
    std::string load_error;
    base::ScopedNativeLibrary app_library(
        base::LoadNativeLibrary(app_path_, &load_error));
    if (!app_library.is_valid()) {
      LOG(ERROR) << "Failed to load library (error: " << load_error << ")";
      break;
    }

    typedef MojoResult (*MojoMainFunction)(MojoHandle);
    MojoMainFunction main_function = reinterpret_cast<MojoMainFunction>(
        app_library.GetFunctionPointer("MojoMain"));
    if (!main_function) {
      LOG(ERROR) << "Entrypoint MojoMain not found";
      break;
    }

    // |MojoMain()| takes ownership of the service handle.
    MojoResult result = main_function(service_handle_.release().value());
    if (result < MOJO_RESULT_OK)
      LOG(ERROR) << "MojoMain returned an error: " << result;
  } while (false);

  app_completed_callback_.Run();
  app_completed_callback_.Reset();
}

}  // namespace shell
}  // namespace mojo
