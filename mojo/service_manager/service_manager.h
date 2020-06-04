// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICE_MANAGER_SERVICE_MANAGER_H_
#define MOJO_SERVICE_MANAGER_SERVICE_MANAGER_H_

#include <map>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/service_manager/service_manager_export.h"
#include "url/gurl.h"

namespace mojo {

class MOJO_SERVICE_MANAGER_EXPORT ServiceManager {
 public:
  // API for testing.
  class MOJO_SERVICE_MANAGER_EXPORT TestAPI {
   public:
    explicit TestAPI(ServiceManager* manager);
    ~TestAPI();

    // Returns a handle to the shell.
    ScopedShellHandle GetShellHandle();

    // Returns true if the shared instance has been created.
    static bool HasCreatedInstance();
    // Returns true if there is a ServiceFactory for this URL.
    bool HasFactoryForURL(const GURL& url) const;

   private:
    class TestShellConnection;

    ServiceManager* manager_;

    scoped_ptr<TestShellConnection> shell_connection_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  // Interface class for debugging only.
  class Interceptor {
   public:
    virtual ~Interceptor() {}
    // Called when ServiceManager::Connect is called.
    virtual ScopedMessagePipeHandle OnConnectToClient(
        const GURL& url, ScopedMessagePipeHandle handle) = 0;
  };

  ServiceManager();
  ~ServiceManager();

  // Returns a shared instance, creating it if necessary.
  static ServiceManager* GetInstance();

  // Loads a service if necessary and establishes a new client connection.
  void Connect(const GURL& url, ScopedMessagePipeHandle client_handle);

  // Sets the default Loader to be used if not overridden by SetLoaderForURL()
  // or SetLoaderForScheme().
  void set_default_loader(scoped_ptr<ServiceLoader> loader) {
    default_loader_ = loader.Pass();
  }
  // Sets a Loader to be used for a specific url.
  void SetLoaderForURL(scoped_ptr<ServiceLoader> loader, const GURL& url);
  // Sets a Loader to be used for a specific url scheme.
  void SetLoaderForScheme(scoped_ptr<ServiceLoader> loader,
                          const std::string& scheme);
  // Allows to interpose a debugger to service connections.
  void SetInterceptor(Interceptor* interceptor);

 private:
  class ServiceFactory;
  typedef std::map<std::string, ServiceLoader*> SchemeToLoaderMap;
  typedef std::map<GURL, ServiceLoader*> URLToLoaderMap;
  typedef std::map<GURL, ServiceFactory*> URLToServiceFactoryMap;

  // Returns the Loader to use for a url (using default if not overridden.)
  // The preference is to use a loader that's been specified for an url first,
  // then one that's been specified for a scheme, then the default.
  ServiceLoader* GetLoaderForURL(const GURL& url);

  // Removes a ServiceFactory when it no longer has any connections.
  void OnServiceFactoryError(ServiceFactory* service_factory);

  // Loader management.
  URLToLoaderMap url_to_loader_;
  SchemeToLoaderMap scheme_to_loader_;
  scoped_ptr<ServiceLoader> default_loader_;
  Interceptor* interceptor_;

  URLToServiceFactoryMap url_to_service_factory_;
  DISALLOW_COPY_AND_ASSIGN(ServiceManager);
};

}  // namespace mojo

#endif  // MOJO_SERVICE_MANAGER_SERVICE_MANAGER_H_
