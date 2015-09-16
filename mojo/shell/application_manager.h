// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_APPLICATION_MANAGER_H_
#define MOJO_SHELL_APPLICATION_MANAGER_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "mojo/application/public/interfaces/application.mojom.h"
#include "mojo/application/public/interfaces/service_provider.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/public/cpp/bindings/interface_ptr_info.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/services/updater/updater.mojom.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/capability_filter.h"
#include "mojo/shell/connect_to_application_params.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/identity.h"
#include "mojo/shell/native_runner.h"
#include "url/gurl.h"

namespace base {
class FilePath;
class SequencedWorkerPool;
}

namespace mojo {
namespace shell {

class ApplicationFetcher;
class ApplicationInstance;
class ContentHandlerConnection;

class ApplicationManager {
 public:
  // API for testing.
  class TestAPI {
   public:
    explicit TestAPI(ApplicationManager* manager);
    ~TestAPI();

    // Returns true if the shared instance has been created.
    static bool HasCreatedInstance();
    // Returns true if there is a ApplicationInstance for this URL.
    bool HasRunningInstanceForURL(const GURL& url) const;
   private:
    ApplicationManager* manager_;

    DISALLOW_COPY_AND_ASSIGN(TestAPI);
  };

  explicit ApplicationManager(scoped_ptr<ApplicationFetcher> fetcher);
  ~ApplicationManager();

  // Loads a service if necessary and establishes a new client connection.
  // |originator| can be NULL (e.g. for the first application or in tests), but
  // typically is non-NULL and identifies the instance initiating the
  // connection.
  // Please see the comments in connect_to_application_params.h for more details
  // about the parameters.
  void ConnectToApplication(
      ApplicationInstance* originator,
      URLRequestPtr app_url_request,
      const std::string& qualifier,
      InterfaceRequest<ServiceProvider> services,
      ServiceProviderPtr exposed_services,
      const CapabilityFilter& filter,
      const base::Closure& on_application_end,
      const Shell::ConnectToApplicationCallback& connect_callback);

  void ConnectToApplication(scoped_ptr<ConnectToApplicationParams> params);

  // Must only be used by shell internals and test code as it does not forward
  // capability filters.
  template <typename Interface>
  inline void ConnectToService(const GURL& application_url,
                               InterfacePtr<Interface>* ptr) {
    ScopedMessagePipeHandle service_handle =
        ConnectToServiceByName(application_url, Interface::Name_);
    ptr->Bind(InterfacePtrInfo<Interface>(service_handle.Pass(), 0u));
  }

  void RegisterContentHandler(const std::string& mime_type,
                              const GURL& content_handler_url);

  // Registers a package alias. When attempting to load |alias|, it will
  // instead redirect to |content_handler_package|, which is a content handler
  // which will be passed the |alias| as the URLResponse::url. Different values
  // of |alias| with the same |qualifier| that are in the same
  // |content_handler_package| will run in the same process in multi-process
  // mode.
  void RegisterApplicationPackageAlias(const GURL& alias,
                                       const GURL& content_handler_package,
                                       const std::string& qualifier);

  // Sets the default Loader to be used if not overridden by SetLoaderForURL().
  void set_default_loader(scoped_ptr<ApplicationLoader> loader) {
    default_loader_ = loader.Pass();
  }
  void set_native_runner_factory(
      scoped_ptr<NativeRunnerFactory> runner_factory) {
    native_runner_factory_ = runner_factory.Pass();
  }
  void set_blocking_pool(base::SequencedWorkerPool* blocking_pool) {
    blocking_pool_ = blocking_pool;
  }
  // Sets a Loader to be used for a specific url.
  void SetLoaderForURL(scoped_ptr<ApplicationLoader> loader, const GURL& url);

  // Destroys all Shell-ends of connections established with Applications.
  // Applications connected by this ApplicationManager will observe pipe errors
  // and have a chance to shutdown.
  void TerminateShellConnections();

  // Removes a ApplicationInstance when it encounters an error.
  void OnApplicationInstanceError(ApplicationInstance* instance);

  // Removes a ContentHandler when its connection is closed.
  void OnContentHandlerConnectionClosed(
      ContentHandlerConnection* content_handler);

  ApplicationInstance* GetApplicationInstance(const Identity& identity) const;

 private:
  using ApplicationPackagedAlias = std::map<GURL, std::pair<GURL, std::string>>;
  using IdentityToApplicationInstanceMap =
      std::map<Identity, ApplicationInstance*>;
  using MimeTypeToURLMap = std::map<std::string, GURL>;
  using URLToContentHandlerMap =
      std::map<std::pair<GURL, std::string>, ContentHandlerConnection*>;
  using URLToLoaderMap = std::map<GURL, ApplicationLoader*>;

  // Takes the contents of |params| only when it returns true.
  bool ConnectToRunningApplication(
      scoped_ptr<ConnectToApplicationParams>* params);
  // |resolved_url| is the URL to load by |loader| (if loader is not null). It
  // may be different from |(*params)->app_url()| because of mappings and
  // resolution rules.
  // Takes the contents of |params| only when it returns true.
  void ConnectToApplicationWithLoader(
      scoped_ptr<ConnectToApplicationParams>* params,
      const GURL& resolved_url,
      ApplicationLoader* loader);

  InterfaceRequest<Application> RegisterInstance(
      scoped_ptr<ConnectToApplicationParams> params,
      ApplicationInstance** resulting_instance);

  // Called once |fetcher| has found app. |params->app_url()| is the url of
  // the requested application before any mappings/resolution have been applied.
  // The corresponding URLRequest struct in |params| has been taken.
  void HandleFetchCallback(scoped_ptr<ConnectToApplicationParams> params,
                           scoped_ptr<Fetcher> fetcher);

  void RunNativeApplication(InterfaceRequest<Application> application_request,
                            bool start_sandboxed,
                            scoped_ptr<Fetcher> fetcher,
                            const base::FilePath& file_path,
                            bool path_exists);

  void LoadWithContentHandler(
      const Identity& originator_identity,
      const CapabilityFilter& originator_filter,
      const GURL& content_handler_url,
      const std::string& qualifier,
      const CapabilityFilter& filter,
      const Shell::ConnectToApplicationCallback& connect_callback,
      ApplicationInstance* app,
      InterfaceRequest<Application> application_request,
      URLResponsePtr url_response);

  // Returns the appropriate loader for |url|, or the default loader if there is
  // no loader configured for the URL.
  ApplicationLoader* GetLoaderForURL(const GURL& url);

  void CleanupRunner(NativeRunner* runner);

  ScopedMessagePipeHandle ConnectToServiceByName(
      const GURL& application_url,
      const std::string& interface_name);

  scoped_ptr<ApplicationFetcher> const fetcher_;
  // Loader management.
  // Loaders are chosen in the order they are listed here.
  URLToLoaderMap url_to_loader_;
  scoped_ptr<ApplicationLoader> default_loader_;
  scoped_ptr<NativeRunnerFactory> native_runner_factory_;

  ApplicationPackagedAlias application_package_alias_;
  IdentityToApplicationInstanceMap identity_to_instance_;
  URLToContentHandlerMap url_to_content_handler_;

  base::SequencedWorkerPool* blocking_pool_;
  updater::UpdaterPtr updater_;
  MimeTypeToURLMap mime_type_to_url_;
  ScopedVector<NativeRunner> native_runners_;
  // Counter used to assign ids to content_handlers.
  uint32_t content_handler_id_counter_;
  base::WeakPtrFactory<ApplicationManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationManager);
};

Shell::ConnectToApplicationCallback EmptyConnectCallback();

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_APPLICATION_MANAGER_H_
