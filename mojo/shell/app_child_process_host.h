// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_
#define MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_

#include "base/macros.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/shell/app_child_process.mojom.h"
#include "mojo/shell/child_process_host.h"

namespace mojo {

namespace embedder {
struct ChannelInfo;
}

namespace shell {

// Note: After |Start()|, this object must remain alive until the controller
// client's |AppCompleted()| is called.
class AppChildProcessHost : public ChildProcessHost,
                            public ChildProcessHost::Delegate {
 public:
  AppChildProcessHost(Context* context,
                      mojo_shell::AppChildControllerClient* controller_client);
  virtual ~AppChildProcessHost();

  mojo_shell::AppChildController* controller() {
    return controller_.get();
  }

 private:
  // |ChildProcessHost::Delegate| methods:
  virtual void WillStart() OVERRIDE;
  virtual void DidStart(bool success) OVERRIDE;

  // Callback for |embedder::CreateChannel()|.
  void DidCreateChannel(embedder::ChannelInfo* channel_info);

  mojo_shell::AppChildControllerClient* const controller_client_;

  RemotePtr<mojo_shell::AppChildController> controller_;
  embedder::ChannelInfo* channel_info_;

  DISALLOW_COPY_AND_ASSIGN(AppChildProcessHost);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APP_CHILD_PROCESS_HOST_H_
