// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_SERVICE_MANAGER_H_
#define MOJO_SHELL_SERVICE_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "mojo/public/system/core_cpp.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

class ServiceManager {
 public:
  // Interface to allowing default loading behavior to be overridden for a
  // specific url.
  class Loader {
   public:
    virtual ~Loader();
    virtual void Load(const GURL& url,
                      ScopedMessagePipeHandle service_handle) = 0;
   protected:
    Loader();
  };

  ServiceManager();
  ~ServiceManager();

  // Sets the default Loader to be used if not overridden by SetLoaderForURL().
  // Does not take ownership of |loader|.
  void set_default_loader(Loader* loader) { default_loader_ = loader; }
  // Sets a Loader to be used for a specific url.
  // Does not take ownership of |loader|.
  void SetLoaderForURL(Loader* loader, const GURL& gurl);
  // Returns the Loader to use for a url (using default if not overridden.)
  Loader* GetLoaderForURL(const GURL& gurl);
  // Loads a service if necessary and establishes a new client connection.
  void Connect(const GURL& url, ScopedMessagePipeHandle client_handle);

 private:
  class Service;

  Loader* default_loader_;
  typedef std::map<GURL, Service*> ServiceMap;
  ServiceMap url_to_service_;
  typedef std::map<GURL, Loader*> LoaderMap;
  LoaderMap url_to_loader_;
  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_SERVICE_MANAGER_H_
