// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_COMPOSITOR_APP_GLES2_CLIENT_IMPL_H_
#define MOJO_EXAMPLES_COMPOSITOR_APP_GLES2_CLIENT_IMPL_H_

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "mojo/public/bindings/remote_ptr.h"
#include "mojo/public/gles2/gles2.h"
#include "mojom/native_viewport.h"

namespace gpu {
class ContextSupport;
namespace gles2 {
class GLES2Interface;
}
}

namespace mojo {
namespace examples {

class GLES2ClientImpl {
 public:
  explicit GLES2ClientImpl(ScopedMessagePipeHandle pipe);
  virtual ~GLES2ClientImpl();

  gpu::gles2::GLES2Interface* Interface() const;
  gpu::ContextSupport* Support() const;

 private:
  void ContextLost();
  static void ContextLostThunk(void* closure);

  MojoGLES2Context context_;

  MOJO_DISALLOW_COPY_AND_ASSIGN(GLES2ClientImpl);
};

}  // namespace examples
}  // namespace mojo

#endif  // MOJO_EXAMPLES_COMPOSITOR_APP_GLES2_CLIENT_IMPL_H_
