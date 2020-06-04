// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/application_manager.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/services/package_manager/loader.h"
#include "mojo/shell/application_instance.h"
#include "mojo/shell/fetcher.h"
#include "mojo/shell/package_manager.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/query_util.h"
#include "mojo/shell/shell_application_loader.h"
#include "mojo/shell/switches.h"
#include "mojo/util/filename_util.h"

namespace mojo {
namespace shell {

namespace {

// Used by TestAPI.
bool has_created_instance = false;

void OnEmptyOnConnectCallback(uint32_t remote_id, uint32_t content_handler_id) {
}

}  // namespace

// static
ApplicationManager::TestAPI::TestAPI(ApplicationManager* manager)
    : manager_(manager) {
}

ApplicationManager::TestAPI::~TestAPI() {
}

bool ApplicationManager::TestAPI::HasCreatedInstance() {
  return has_created_instance;
}

bool ApplicationManager::TestAPI::HasRunningInstanceForURL(
    const GURL& url) const {
  return manager_->identity_to_instance_.find(Identity(url)) !=
         manager_->identity_to_instance_.end();
}

ApplicationManager::ApplicationManager(
    scoped_ptr<PackageManager> package_manager)
    : ApplicationManager(std::move(package_manager), nullptr, nullptr) {}

ApplicationManager::ApplicationManager(
    scoped_ptr<PackageManager> package_manager,
    scoped_ptr<NativeRunnerFactory> native_runner_factory,
    base::TaskRunner* task_runner)
    : use_remote_package_manager_(false),
      package_manager_(std::move(package_manager)),
      task_runner_(task_runner),
      native_runner_factory_(std::move(native_runner_factory)),
      weak_ptr_factory_(this) {
  package_manager_->SetApplicationManager(this);
  SetLoaderForURL(make_scoped_ptr(new ShellApplicationLoader(this)),
                  GURL("mojo:shell"));
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
         switches::kUseRemotePackageManager)) {
    UseRemotePackageManager();
  }
}

ApplicationManager::~ApplicationManager() {
  TerminateShellConnections();
  STLDeleteValues(&url_to_loader_);
  for (auto& runner : native_runners_)
    runner.reset();
}

void ApplicationManager::TerminateShellConnections() {
  STLDeleteValues(&identity_to_instance_);
}

void ApplicationManager::ConnectToApplication(
    scoped_ptr<ConnectToApplicationParams> params) {
  TRACE_EVENT_INSTANT1("mojo_shell", "ApplicationManager::ConnectToApplication",
                       TRACE_EVENT_SCOPE_THREAD, "original_url",
                       params->target().url().spec());
  DCHECK(params->target().url().is_valid());

  // Connect to an existing matching instance, if possible.
  if (ConnectToRunningApplication(&params))
    return;

  ApplicationLoader* loader = GetLoaderForURL(params->target().url());
  if (loader) {
    GURL url = params->target().url();
    package_manager_->BuiltinAppLoaded(url);
    mojom::ShellClientRequest request;
    std::string application_name =
        package_manager_->GetApplicationName(params->target().url().spec());
    ApplicationInstance* instance = CreateAndConnectToInstance(
        std::move(params), nullptr, nullptr, application_name, &request);
    loader->Load(url, std::move(request));
    instance->RunConnectCallback();
    return;
  }

  if (use_remote_package_manager_) {
    std::string url = params->target().url().spec();
    shell_resolver_->ResolveMojoURL(
        url,
        base::Bind(&ApplicationManager::OnGotResolvedURL,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&params)));
  } else {
    URLRequestPtr original_url_request = params->TakeTargetURLRequest();
    auto callback =
      base::Bind(&ApplicationManager::HandleFetchCallback,
      weak_ptr_factory_.GetWeakPtr(), base::Passed(&params));
    package_manager_->FetchRequest(std::move(original_url_request), callback);
  }
}

void ApplicationManager::UseRemotePackageManager() {
  use_remote_package_manager_ = true;

  GURL package_manager_url("mojo://package_manager/");

  SetLoaderForURL(make_scoped_ptr(new package_manager::Loader(task_runner_)),
                  package_manager_url);

  shell::mojom::InterfaceProviderPtr interfaces;
  scoped_ptr<ConnectToApplicationParams> params(new ConnectToApplicationParams);
  params->set_source(Identity(GURL("mojo:shell"), std::string(),
                     GetPermissiveCapabilityFilter()));
  params->set_remote_interfaces(GetProxy(&interfaces));

  if (false) {
    GURL file_url =
        package_manager_->ResolveMojoURL(package_manager_url);
    params->SetTarget(Identity(file_url, std::string(),
                      GetPermissiveCapabilityFilter()));
    // TODO(beng): Fish the name out of a manifest. There's a chicken-and-egg
    // issue here as the package manager reads the manifests. The solution is
    //probably to defer application name loading.
    CreateAndRunLocalApplication(std::move(params), "Package Manager",
                                 file_url);
  } else {
    params->SetTarget(Identity(package_manager_url, std::string(),
                               GetPermissiveCapabilityFilter()));
    ConnectToApplication(std::move(params));
  }
  GetInterface(interfaces.get(), &shell_resolver_);
}

bool ApplicationManager::ConnectToRunningApplication(
    scoped_ptr<ConnectToApplicationParams>* params) {
  ApplicationInstance* instance = GetApplicationInstance((*params)->target());
  if (!instance)
    return false;

  // TODO(beng): CHECK() that the target URL is already in the application
  //             catalog.
  instance->ConnectToClient(std::move(*params));
  return true;
}

ApplicationInstance* ApplicationManager::GetApplicationInstance(
    const Identity& identity) const {
  const auto& it = identity_to_instance_.find(identity);
  return it != identity_to_instance_.end() ? it->second : nullptr;
}

void ApplicationManager::CreateInstanceForHandle(
    ScopedHandle channel,
    const GURL& url,
    mojom::CapabilityFilterPtr filter,
    InterfaceRequest<mojom::PIDReceiver> pid_receiver) {
  // We don't call ConnectToClient() here since the instance was created
  // manually by other code, not in response to a Connect() request. The newly
  // created instance is identified by |url| and may be subsequently reached by
  // client code using this identity.
  CapabilityFilter local_filter = filter->filter.To<CapabilityFilter>();
  Identity target_id(url, std::string(), local_filter);
  mojom::ShellClientRequest request;
  ApplicationInstance* instance = CreateInstance(
      target_id, EmptyConnectCallback(), base::Closure(),
      package_manager_->GetApplicationName(url.spec()), &request);
  instance->BindPIDReceiver(std::move(pid_receiver));
  scoped_ptr<NativeRunner> runner =
      native_runner_factory_->Create(base::FilePath());
  runner->InitHost(std::move(channel), std::move(request));
  instance->SetNativeRunner(runner.get());
  native_runners_.push_back(std::move(runner));
}

void ApplicationManager::AddListener(
    mojom::ApplicationManagerListenerPtr listener) {
  Array<mojom::ApplicationInfoPtr> applications;
  for (auto& entry : identity_to_instance_)
    applications.push_back(CreateApplicationInfoForInstance(entry.second));
  listener->SetRunningApplications(std::move(applications));

  listeners_.AddInterfacePtr(std::move(listener));
}

void ApplicationManager::ApplicationPIDAvailable(
    uint32_t id,
    base::ProcessId pid) {
  for (auto& instance : identity_to_instance_) {
    if (instance.second->id() == id) {
      instance.second->set_pid(pid);
      break;
    }
  }
  listeners_.ForAllPtrs(
      [this, id, pid](mojom::ApplicationManagerListener* listener) {
        listener->ApplicationPIDAvailable(id, pid);
      });
}

ApplicationInstance* ApplicationManager::CreateAndConnectToInstance(
    scoped_ptr<ConnectToApplicationParams> params,
    Identity* source,
    Identity* target,
    const std::string& application_name,
    mojom::ShellClientRequest* request) {
  if (source)
    *source = params->source();
  if (target)
    *target = params->target();
  ApplicationInstance* instance = CreateInstance(
      params->target(), params->connect_callback(),
      params->on_application_end(),
      application_name,
      request);
  params->set_connect_callback(EmptyConnectCallback());
  instance->ConnectToClient(std::move(params));
  return instance;
}

ApplicationInstance* ApplicationManager::CreateInstance(
    const Identity& target_id,
    const mojom::Shell::ConnectToApplicationCallback& connect_callback,
    const base::Closure& on_application_end,
    const String& application_name,
    mojom::ShellClientRequest* request) {
  mojom::ShellClientPtr shell_client;
  *request = GetProxy(&shell_client);
  ApplicationInstance* instance = new ApplicationInstance(
      std::move(shell_client), this, target_id,
      mojom::Shell::kInvalidApplicationID, connect_callback,on_application_end,
      application_name);
  DCHECK(identity_to_instance_.find(target_id) ==
         identity_to_instance_.end());
  identity_to_instance_[target_id] = instance;
  mojom::ApplicationInfoPtr application_info =
      CreateApplicationInfoForInstance(instance);
  listeners_.ForAllPtrs(
      [this, &application_info](mojom::ApplicationManagerListener* listener) {
        listener->ApplicationInstanceCreated(application_info.Clone());
      });
  instance->InitializeApplication();
  return instance;
}

void ApplicationManager::OnGotResolvedURL(
    scoped_ptr<ConnectToApplicationParams> params,
    const String& resolved_url,
    const String& file_url,
    const String& application_name,
    mojom::CapabilityFilterPtr base_filter) {
  // It's possible that when this manifest request was issued, another one was
  // already in-progress and completed by the time this one did, and so the
  // requested application may already be running.
  if (ConnectToRunningApplication(&params))
    return;

  GURL resolved_gurl = resolved_url.To<GURL>();
  if (params->target().url().spec() != resolved_url) {
    CapabilityFilter capability_filter = GetPermissiveCapabilityFilter();
    if (!base_filter.is_null())
      capability_filter = base_filter->filter.To<CapabilityFilter>();

    // TODO(beng): For now, we just use the legacy PackageManagerImpl to manage
    //             the ContentHandler connection. Once we get rid of the
    //             non-remote package manager path we will have to fold this in
    //             here.
    Identity source, target;
    mojom::ShellClientRequest request;
    ApplicationInstance* instance = CreateAndConnectToInstance(
        std::move(params), &source, &target, application_name, &request);

    uint32_t content_handler_id = package_manager_->StartContentHandler(
        source, Identity(resolved_gurl, target.qualifier(), capability_filter),
        target.url(), std::move(request));
    CHECK(content_handler_id != mojom::Shell::kInvalidApplicationID);
    instance->set_requesting_content_handler_id(content_handler_id);
    instance->RunConnectCallback();
    return;
  }
  CreateAndRunLocalApplication(std::move(params), application_name,
                               file_url.To<GURL>());
}

void ApplicationManager::CreateAndRunLocalApplication(
    scoped_ptr<ConnectToApplicationParams> params,
    const String& application_name,
    const GURL& file_url) {
  Identity source, target;
  mojom::ShellClientRequest request;
  ApplicationInstance* instance = CreateAndConnectToInstance(
      std::move(params), &source, &target, application_name, &request);

  scoped_ptr<Fetcher> fetcher;
  bool start_sandboxed = false;
  RunNativeApplication(std::move(request), start_sandboxed, std::move(fetcher),
                       instance, util::UrlToFilePath(file_url), true);
  instance->RunConnectCallback();
}

void ApplicationManager::HandleFetchCallback(
    scoped_ptr<ConnectToApplicationParams> params,
    scoped_ptr<Fetcher> fetcher) {
  if (!fetcher) {
    // Network error. Drop |params| to tell the requestor.
    params->connect_callback().Run(mojom::Shell::kInvalidApplicationID,
                                   mojom::Shell::kInvalidApplicationID);
    return;
  }

  GURL redirect_url = fetcher->GetRedirectURL();
  if (!redirect_url.is_empty()) {
    // And around we go again... Whee!
    // TODO(sky): this loses the original URL info.
    URLRequestPtr new_request = URLRequest::New();
    new_request->url = redirect_url.spec();
    HttpHeaderPtr header = HttpHeader::New();
    header->name = "Referer";
    header->value = fetcher->GetRedirectReferer().spec();
    new_request->headers.push_back(std::move(header));
    params->SetTargetURLRequest(std::move(new_request));
    ConnectToApplication(std::move(params));
    return;
  }

  // We already checked if the application was running before we fetched it, but
  // it might have started while the fetch was outstanding. We don't want to
  // have two copies of the app running, so check again.
  if (ConnectToRunningApplication(&params))
    return;

  Identity source, target;
  mojom::ShellClientRequest request;
  std::string application_name =
      package_manager_->GetApplicationName(params->target().url().spec());
  ApplicationInstance* instance = CreateAndConnectToInstance(
      std::move(params), &source, &target, application_name, &request);

  uint32_t content_handler_id = package_manager_->HandleWithContentHandler(
      fetcher.get(), source, target.url(), target.filter(), &request);
  if (content_handler_id != mojom::Shell::kInvalidApplicationID) {
    instance->set_requesting_content_handler_id(content_handler_id);
  } else {
    bool start_sandboxed = false;
    fetcher->AsPath(
        task_runner_,
        base::Bind(&ApplicationManager::RunNativeApplication,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(std::move(request)), start_sandboxed,
                   base::Passed(std::move(fetcher)),
                   base::Unretained(instance)));
  }
  instance->RunConnectCallback();
}

void ApplicationManager::RunNativeApplication(
    InterfaceRequest<mojom::ShellClient> request,
    bool start_sandboxed,
    scoped_ptr<Fetcher> fetcher,
    ApplicationInstance* instance,
    const base::FilePath& path,
    bool path_exists) {
  // We only passed fetcher to keep it alive. Done with it now.
  fetcher.reset();

  DCHECK(request.is_pending());

  if (!path_exists) {
    LOG(ERROR) << "Library not started because library path '" << path.value()
               << "' does not exist.";
    return;
  }

  TRACE_EVENT1("mojo_shell", "ApplicationManager::RunNativeApplication", "path",
               path.AsUTF8Unsafe());
  scoped_ptr<NativeRunner> runner = native_runner_factory_->Create(path);
  runner->Start(path, start_sandboxed, std::move(request),
                base::Bind(&ApplicationManager::ApplicationPIDAvailable,
                           weak_ptr_factory_.GetWeakPtr(), instance->id()),
                base::Bind(&ApplicationManager::CleanupRunner,
                           weak_ptr_factory_.GetWeakPtr(), runner.get()));
  instance->SetNativeRunner(runner.get());
  native_runners_.push_back(std::move(runner));
}

void ApplicationManager::SetLoaderForURL(scoped_ptr<ApplicationLoader> loader,
                                         const GURL& url) {
  URLToLoaderMap::iterator it = url_to_loader_.find(url);
  if (it != url_to_loader_.end())
    delete it->second;
  url_to_loader_[url] = loader.release();
}

ApplicationLoader* ApplicationManager::GetLoaderForURL(const GURL& url) {
  auto url_it = url_to_loader_.find(GetBaseURLAndQuery(url, nullptr));
  if (url_it != url_to_loader_.end())
    return url_it->second;
  return default_loader_.get();
}

mojom::ApplicationInfoPtr ApplicationManager::CreateApplicationInfoForInstance(
    ApplicationInstance* instance) const {
  mojom::ApplicationInfoPtr info(mojom::ApplicationInfo::New());
  info->id = instance->id();
  info->url = instance->identity().url().spec();
  info->qualifier = instance->identity().qualifier();
  if (use_remote_package_manager_)
    info->name = instance->application_name();
  else
    info->name = package_manager_->GetApplicationName(info->url);
  if (instance->identity().url().spec() == "mojo://shell/")
    info->pid = base::Process::Current().Pid();
  else
    info->pid = instance->pid();
  return info;
}

void ApplicationManager::OnApplicationInstanceError(
    ApplicationInstance* instance) {
  // Called from ~ApplicationInstance, so we do not need to call Destroy here.
  const Identity identity = instance->identity();
  base::Closure on_application_end = instance->on_application_end();
  // Remove the shell.
  auto it = identity_to_instance_.find(identity);
  DCHECK(it != identity_to_instance_.end());
  int id = instance->id();
  delete it->second;
  identity_to_instance_.erase(it);
  listeners_.ForAllPtrs(
      [this, id](mojom::ApplicationManagerListener* listener) {
        listener->ApplicationInstanceDestroyed(id);
      });
  if (!on_application_end.is_null())
    on_application_end.Run();
}

void ApplicationManager::CleanupRunner(NativeRunner* runner) {
  for (auto it = native_runners_.begin(); it != native_runners_.end(); ++it) {
    if (it->get() == runner) {
      native_runners_.erase(it);
      return;
    }
  }
}

mojom::Shell::ConnectToApplicationCallback EmptyConnectCallback() {
  return base::Bind(&OnEmptyOnConnectCallback);
}

}  // namespace shell
}  // namespace mojo
