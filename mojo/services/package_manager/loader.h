// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_PACKAGE_MANAGER_LOADER_H_
#define MOJO_SERVICES_PACKAGE_MANAGER_LOADER_H_

#include "mojo/shell/application_loader.h"

namespace base {
class BlockingPool;
}

namespace mojo {
class ShellClient;
class ShellConnection;
}

namespace package_manager {

class Loader : public mojo::shell::ApplicationLoader {
 public:
  explicit Loader(base::TaskRunner* blocking_pool);
  ~Loader() override;

  // mojo::shell::ApplicationLoader:
  void Load(const GURL& url,
            mojo::shell::mojom::ShellClientRequest request) override;

 private:
  base::TaskRunner* blocking_pool_;
  scoped_ptr<mojo::ShellClient> client_;
  scoped_ptr<mojo::ShellConnection> connection_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

}  // namespace package_manager

#endif  // MOJO_SERVICES_PACKAGE_MANAGER_LOADER_H_
