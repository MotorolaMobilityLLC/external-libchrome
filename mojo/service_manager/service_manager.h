// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICE_MANAGER_SERVICE_MANAGER_H_
#define MOJO_SERVICE_MANAGER_SERVICE_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "mojo/public/shell/shell.mojom.h"
#include "url/gurl.h"

namespace mojo {

class ServiceLoader;

class ServiceManager {
 public:
  // API for testing.
  class TestAPI {
   private:
    friend class ServiceManagerTest;
    explicit TestAPI(ServiceManager* manager) : manager_(manager) {}
    // Returns true if there is a ServiceFactory for this URL.
    bool HasFactoryForURL(const GURL& url) const;

    ServiceManager* manager_;
  };

  ServiceManager();
  ~ServiceManager();

  // Returns a shared instance, creating it if necessary.
  static ServiceManager* GetInstance();

  // Sets the default Loader to be used if not overridden by SetLoaderForURL().
  // Does not take ownership of |loader|.
  void set_default_loader(ServiceLoader* loader) { default_loader_ = loader; }
  // Sets a Loader to be used for a specific url.
  // Does not take ownership of |loader|.
  void SetLoaderForURL(ServiceLoader* loader, const GURL& gurl);
  // Returns the Loader to use for a url (using default if not overridden.)
  ServiceLoader* GetLoaderForURL(const GURL& gurl);
  // Loads a service if necessary and establishes a new client connection.
  void Connect(const GURL& url, ScopedMessagePipeHandle client_handle);

 private:
  class ServiceFactory;

  // Removes a ServiceFactory when it no longer has any connections.
  void RemoveServiceFactory(ServiceFactory* service_factory);

  ServiceLoader* default_loader_;
  typedef std::map<GURL, ServiceFactory*> ServiceFactoryMap;
  ServiceFactoryMap url_to_service_factory_;
  typedef std::map<GURL, ServiceLoader*> LoaderMap;
  LoaderMap url_to_loader_;
  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace mojo

#endif  // MOJO_SERVICE_MANAGER_SERVICE_MANAGER_H_
