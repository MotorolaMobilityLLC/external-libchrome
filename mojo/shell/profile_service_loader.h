// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PROFILE_SERVICE_LOADER_H_
#define MOJO_SHELL_PROFILE_SERVICE_LOADER_H_

#include <map>

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/service_manager/service_loader.h"

namespace mojo {

class ApplicationImpl;

namespace shell {

// ServiceLoader responsible for creating connections to the ProfileService.
class ProfileServiceLoader : public ServiceLoader, public ApplicationDelegate {
 public:
  ProfileServiceLoader();
  virtual ~ProfileServiceLoader();

 private:
  // ServiceLoader overrides:
  virtual void LoadService(
      ServiceManager* manager,
      const GURL& url,
      ScopedMessagePipeHandle shell_handle) OVERRIDE;
  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE;

  // ApplicationDelegate overrides.
  virtual bool ConfigureIncomingConnection(ApplicationConnection* connection)
      MOJO_OVERRIDE;

  base::ScopedPtrHashMap<uintptr_t, ApplicationImpl> apps_;

  DISALLOW_COPY_AND_ASSIGN(ProfileServiceLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_PROFILE_SERVICE_LOADER_H_
