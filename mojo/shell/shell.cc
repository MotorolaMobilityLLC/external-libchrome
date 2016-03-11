// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/shell.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "mojo/common/url_type_converters.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/services/catalog/loader.h"
#include "mojo/shell/connect_util.h"
#include "mojo/shell/public/cpp/connect.h"
#include "mojo/shell/public/cpp/names.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/public/interfaces/connector.mojom.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client.mojom.h"
#include "mojo/util/filename_util.h"

namespace mojo {
namespace shell {
namespace {
const char kCatalogName[] = "mojo:catalog";

void EmptyResolverCallback(const String& resolved_name,
                           const String& resolved_instance,
                           mojom::CapabilitySpecPtr capabilities,
                           const String& file_url) {}

}

Identity CreateShellIdentity() {
  return Identity("mojo:shell", mojom::kRootUserID);
}

CapabilitySpec GetPermissiveCapabilities() {
  CapabilitySpec capabilities;
  CapabilityRequest spec;
  spec.interfaces.insert("*");
  capabilities.required["*"] = spec;
  return capabilities;
}

CapabilityRequest GetCapabilityRequest(const CapabilitySpec& source_spec,
                                       const Identity& target) {
  // Start by looking for specs specific to the supplied identity.
  auto it = source_spec.required.find(target.name());
  if (it != source_spec.required.end())
    return it->second;

  // Fall back to looking for a wildcard rule.
  it = source_spec.required.find("*");
  if (source_spec.required.size() == 1 && it != source_spec.required.end())
    return it->second;

  // Finally, nothing is allowed.
  return CapabilityRequest();
}

CapabilityRequest GenerateCapabilityRequestForConnection(
    const CapabilitySpec& source_spec,
    const Identity& target,
    const CapabilitySpec& target_spec) {
  CapabilityRequest request = GetCapabilityRequest(source_spec, target);
  // Flatten all interfaces from classes requested by the source into the
  // allowed interface set in the request.
  for (const auto& class_name : request.classes) {
    auto it = target_spec.provided.find(class_name);
    if (it != target_spec.provided.end()) {
      for (const auto& interface_name : it->second)
        request.interfaces.insert(interface_name);
    }
  }
  return request;
}

// Encapsulates a connection to an instance of an application, tracked by the
// shell's Shell.
class Shell::Instance : public mojom::Connector,
                        public mojom::PIDReceiver,
                        public ShellClient,
                        public InterfaceFactory<mojom::Shell>,
                        public mojom::Shell {
 public:
  Instance(mojom::ShellClientPtr shell_client,
           mojo::shell::Shell* shell,
           const Identity& identity,
           const CapabilitySpec& capability_spec)
    : shell_(shell),
      id_(GenerateUniqueID()),
      identity_(identity),
      capability_spec_(capability_spec),
      allow_any_application_(capability_spec.required.size() == 1 &&
                             capability_spec.required.count("*") == 1),
      shell_client_(std::move(shell_client)),
      pid_receiver_binding_(this),
      weak_factory_(this) {
    if (identity_.name() == "mojo:shell" ||
        shell_->GetLoaderForName(identity_.name())) {
      pid_ = base::Process::Current().Pid();
    }
    DCHECK_NE(mojom::kInvalidInstanceID, id_);
  }

  ~Instance() override {}

  void InitializeClient() {
    shell_client_->Initialize(connectors_.CreateInterfacePtrAndBind(this),
                              mojom::Identity::From(identity_), id_);
    connectors_.set_connection_error_handler(
        base::Bind(&mojo::shell::Shell::OnInstanceError,
                   base::Unretained(shell_), base::Unretained(this)));
  }

  void ConnectToClient(scoped_ptr<ConnectParams> params) {
    params->connect_callback().Run(mojom::ConnectResult::SUCCEEDED,
                                   identity_.user_id(), id_);
    uint32_t source_id = mojom::kInvalidInstanceID;
    CapabilityRequest spec;
    spec.interfaces.insert("*");
    Instance* source = shell_->GetExistingInstance(params->source());
    if (source) {
      spec = GenerateCapabilityRequestForConnection(
          source->capability_spec_, identity_, capability_spec_);
      source_id = source->id();
    }
    shell_client_->AcceptConnection(
        mojom::Identity::From(params->source()), source_id,
        params->TakeRemoteInterfaces(), params->TakeLocalInterfaces(),
        mojom::CapabilityRequest::From(spec), params->target().name());
  }

  void StartWithClientProcessConnection(
      mojom::ShellClientRequest request,
      mojom::ClientProcessConnectionPtr client_process_connection) {
    factory_.Bind(mojom::ShellClientFactoryPtrInfo(
        std::move(client_process_connection->shell_client_factory), 0u));
    pid_receiver_binding_.Bind(
        std::move(client_process_connection->pid_receiver_request));
    factory_->CreateShellClient(std::move(request), identity_.name());
  }

  void StartWithFilePath(mojom::ShellClientRequest request,
                         const base::FilePath& path) {
    scoped_ptr<NativeRunner> runner =
        shell_->native_runner_factory_->Create(path);
    bool start_sandboxed = false;
    runner->Start(path, identity_, start_sandboxed, std::move(request),
                  base::Bind(&Instance::PIDAvailable,
                             weak_factory_.GetWeakPtr()),
                  base::Bind(&mojo::shell::Shell::CleanupRunner,
                             shell_->weak_ptr_factory_.GetWeakPtr(),
                             runner.get()));
    shell_->native_runners_.push_back(std::move(runner));
  }

  mojom::InstanceInfoPtr CreateInstanceInfo() const {
    mojom::InstanceInfoPtr info(mojom::InstanceInfo::New());
    info->id = id_;
    info->identity = mojom::Identity::From(identity_);
    info->pid = pid_;
    return info;
  }

  const Identity& identity() const { return identity_; }
  uint32_t id() const { return id_; }

  // ShellClient:
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<mojom::Shell>(this);
    return true;
  }

 private:
  // mojom::Connector implementation:
  void Connect(mojom::IdentityPtr target_ptr,
               mojom::InterfaceProviderRequest remote_interfaces,
               mojom::InterfaceProviderPtr local_interfaces,
               mojom::ClientProcessConnectionPtr client_process_connection,
               const ConnectCallback& callback) override {
    Identity target = target_ptr.To<Identity>();
    if (!ValidateIdentity(target, callback))
      return;
    if (!ValidateClientProcessConnection(&client_process_connection, target,
                                         callback)) {
      return;
    }
    // TODO(beng): Need to do the following additional policy validation of
    // whether this instance is allowed to connect using:
    // - a user id other than its own, kInheritUserID or kRootUserID.
    // - a non-empty instance name.
    // - a non-null client_process_connection.
    if (!ValidateCapabilities(target, callback))
      return;

    scoped_ptr<ConnectParams> params(new ConnectParams);
    params->set_source(identity_);
    params->set_target(target);
    params->set_remote_interfaces(std::move(remote_interfaces));
    params->set_local_interfaces(std::move(local_interfaces));
    params->set_client_process_connection(std::move(client_process_connection));
    params->set_connect_callback(callback);
    shell_->Connect(std::move(params));
  }
  void Clone(mojom::ConnectorRequest request) override {
    connectors_.AddBinding(this, std::move(request));
  }

  // mojom::PIDReceiver:
  void SetPID(uint32_t pid) override {
    PIDAvailable(pid);
  }

  // InterfaceFactory<mojom::Shell>:
  void Create(Connection* connection,
              mojom::ShellRequest request) override {
    shell_bindings_.AddBinding(this, std::move(request));
  }

  // mojom::Shell implementation:
  void AddInstanceListener(mojom::InstanceListenerPtr listener) override {
    // TODO(beng): this should only track the instances matching this user, and
    // root.
    shell_->AddInstanceListener(std::move(listener));
  }

  bool ValidateIdentity(const Identity& identity,
                        const ConnectCallback& callback) {
    if (!IsValidName(identity.name())) {
      LOG(ERROR) << "Error: invalid Name: " << identity.name();
      callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                   mojom::kInheritUserID, mojom::kInvalidInstanceID);
      return false;
    }
    if (!base::IsValidGUID(identity.user_id())) {
      LOG(ERROR) << "Error: invalid user_id: " << identity.user_id();
      callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                   mojom::kInheritUserID, mojom::kInvalidInstanceID);
      return false;
    }
    return true;
  }

  bool ValidateClientProcessConnection(
      mojom::ClientProcessConnectionPtr* client_process_connection,
      const Identity& identity,
      const ConnectCallback& callback) {
    if (!client_process_connection->is_null()) {
      if (!(*client_process_connection)->shell_client_factory.is_valid() ||
          !(*client_process_connection)->pid_receiver_request.is_valid()) {
        LOG(ERROR) << "Error: must supply both shell_client_factory AND "
                   << "pid_receiver_request when sending "
                   << "client_process_connection.";
        callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                     mojom::kInheritUserID, mojom::kInvalidInstanceID);
        return false;
      }
      if (shell_->GetExistingOrRootInstance(identity)) {
        LOG(ERROR) << "Error: Cannot client process matching existing identity:"
                   << "Name: " << identity.name() << " User: "
                   << identity.user_id() << " Instance: "
                   << identity.instance();
        callback.Run(mojom::ConnectResult::INVALID_ARGUMENT,
                     mojom::kInheritUserID, mojom::kInvalidInstanceID);
        return false;
      }
    }
    return true;
  }

  bool ValidateCapabilities(const Identity& target,
                            const ConnectCallback& callback) {
    if (allow_any_application_ ||
        capability_spec_.required.find(target.name()) !=
            capability_spec_.required.end()) {
      return true;
    }
    LOG(ERROR) << "Capabilities prevented connection from: " <<
                  identity_.name() << " to: " << target.name();
    callback.Run(mojom::ConnectResult::ACCESS_DENIED,
                 mojom::kInheritUserID, mojom::kInvalidInstanceID);
    return false;
  }

  uint32_t GenerateUniqueID() const {
    static uint32_t id = mojom::kInvalidInstanceID;
    ++id;
    CHECK_NE(mojom::kInvalidInstanceID, id);
    return id;
  }

  void PIDAvailable(base::ProcessId pid) {
    pid_ = pid;
    shell_->NotifyPIDAvailable(id_, pid_);
  }

  mojo::shell::Shell* const shell_;
  // An id that identifies this instance. Distinct from pid, as a single process
  // may vend multiple application instances, and this object may exist before a
  // process is launched.
  const uint32_t id_;
  const Identity identity_;
  const CapabilitySpec capability_spec_;
  const bool allow_any_application_;
  mojom::ShellClientPtr shell_client_;
  Binding<mojom::PIDReceiver> pid_receiver_binding_;
  BindingSet<mojom::Connector> connectors_;
  BindingSet<mojom::Shell> shell_bindings_;
  mojom::ShellClientFactoryPtr factory_;
  NativeRunner* runner_ = nullptr;
  base::ProcessId pid_ = base::kNullProcessId;
  base::WeakPtrFactory<Instance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

// static
Shell::TestAPI::TestAPI(Shell* shell) : shell_(shell) {}
Shell::TestAPI::~TestAPI() {}

bool Shell::TestAPI::HasRunningInstanceForName(const std::string& name) const {
  for (const auto& entry : shell_->identity_to_instance_) {
    if (entry.first.name() == name)
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell(scoped_ptr<NativeRunnerFactory> native_runner_factory,
             base::TaskRunner* file_task_runner,
             scoped_ptr<catalog::Store> catalog_store)
    : file_task_runner_(file_task_runner),
      native_runner_factory_(std::move(native_runner_factory)),
      weak_ptr_factory_(this) {
  mojom::ShellClientRequest request;
  CreateInstance(CreateShellIdentity(), GetPermissiveCapabilities(), &request);
  shell_connection_.reset(new ShellConnection(this, std::move(request)));

  InitCatalog(std::move(catalog_store));
}

Shell::~Shell() {
  TerminateShellConnections();
  STLDeleteValues(&name_to_loader_);
  for (auto& runner : native_runners_)
    runner.reset();
}

void Shell::SetInstanceQuitCallback(
    base::Callback<void(const Identity&)> callback) {
  instance_quit_callback_ = callback;
}

void Shell::Connect(scoped_ptr<ConnectParams> params) {
  TRACE_EVENT_INSTANT1("mojo_shell", "Shell::Connect",
                       TRACE_EVENT_SCOPE_THREAD, "original_name",
                       params->target().name());
  DCHECK(IsValidName(params->target().name()));

  if (params->target().user_id() == mojom::kInheritUserID) {
    Instance* source = GetExistingInstance(params->source());
    Identity target = params->target();
    // TODO(beng): we should CHECK source.
    target.set_user_id(source ? source->identity().user_id()
                              : mojom::kRootUserID);
    params->set_target(target);
  }

  CHECK(params->target().user_id() != mojom::kInheritUserID);

  // Connect to an existing matching instance, if possible.
  if (ConnectToExistingInstance(&params))
    return;

  std::string name = params->target().name();
  shell_resolver_->ResolveMojoName(
      name,
      base::Bind(&Shell::OnGotResolvedName,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&params)));
}

mojom::ShellClientRequest Shell::InitInstanceForEmbedder(
    const std::string& name) {
  DCHECK(!embedder_instance_);

  Identity target(name, mojom::kRootUserID);
  DCHECK(!GetExistingInstance(target));

  mojom::ShellClientRequest request;
  embedder_instance_ =
      CreateInstance(target, GetPermissiveCapabilities(), &request);
  DCHECK(embedder_instance_);

  return request;
}

void Shell::SetLoaderForName(scoped_ptr<Loader> loader,
                             const std::string& name) {
  NameToLoaderMap::iterator it = name_to_loader_.find(name);
  if (it != name_to_loader_.end())
    delete it->second;
  name_to_loader_[name] = loader.release();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, ShellClient implementation:

bool Shell::AcceptConnection(Connection* connection) {
  // The only interface we expose is mojom::Shell, and access to this interface
  // is brokered by a policy specific to each caller, managed by the caller's
  // instance. Here we look to see who's calling, and forward to the caller's
  // instance to continue.
  Instance* instance = nullptr;
  for (const auto& entry : identity_to_instance_) {
    if (entry.second->id() == connection->GetRemoteInstanceID()) {
      instance = entry.second;
      break;
    }
  }
  DCHECK(instance);
  return instance->AcceptConnection(connection);
}

////////////////////////////////////////////////////////////////////////////////
// Shell, private:

void Shell::InitCatalog(scoped_ptr<catalog::Store> store) {
  scoped_ptr<Loader> loader(
      new catalog::Loader(file_task_runner_, std::move(store)));
  Loader* loader_raw = loader.get();
  std::string name = kCatalogName;
  SetLoaderForName(std::move(loader), name);

  mojom::ShellClientRequest request;
  // TODO(beng): Does the catalog actually have to be run with a permissive
  //             filter?
  Identity identity(name, mojom::kRootUserID);
  CreateInstance(identity, GetPermissiveCapabilities(), &request);
  loader_raw->Load(name, std::move(request));

  ConnectToInterface(this, CreateShellIdentity(), name, &shell_resolver_);

  // Seed the catalog with manifest info for the shell & catalog.
  if (file_task_runner_) {
    shell_resolver_->ResolveMojoName(name, base::Bind(&EmptyResolverCallback));
    shell_resolver_->ResolveMojoName("mojo:shell",
                                     base::Bind(&EmptyResolverCallback));
  }
}

void Shell::TerminateShellConnections() {
  STLDeleteValues(&identity_to_instance_);
}

void Shell::OnInstanceError(Instance* instance) {
  const Identity identity = instance->identity();
  // Remove the shell.
  auto it = identity_to_instance_.find(identity);
  DCHECK(it != identity_to_instance_.end());
  int id = instance->id();
  delete it->second;
  identity_to_instance_.erase(it);
  instance_listeners_.ForAllPtrs([this, id](mojom::InstanceListener* listener) {
                                   listener->InstanceDestroyed(id);
                                 });
  if (!instance_quit_callback_.is_null())
    instance_quit_callback_.Run(identity);
}

Shell::Instance* Shell::GetExistingInstance(const Identity& identity) const {
  const auto& it = identity_to_instance_.find(identity);
  return it != identity_to_instance_.end() ? it->second : nullptr;
}

Shell::Instance* Shell::GetExistingOrRootInstance(
    const Identity& identity) const {
  Instance* instance = GetExistingInstance(identity);
  if (!instance) {
    Identity root_identity = identity;
    root_identity.set_user_id(mojom::kRootUserID);
    instance = GetExistingInstance(root_identity);
  }
  return instance;
}

void Shell::NotifyPIDAvailable(uint32_t id, base::ProcessId pid) {
  instance_listeners_.ForAllPtrs([id, pid](mojom::InstanceListener* listener) {
                                   listener->InstancePIDAvailable(id, pid);
                                 });
}

bool Shell::ConnectToExistingInstance(scoped_ptr<ConnectParams>* params) {
  Instance* instance = GetExistingOrRootInstance((*params)->target());
  if (instance)
    instance->ConnectToClient(std::move(*params));
  return !!instance;
}

Shell::Instance* Shell::CreateInstance(const Identity& target_id,
                                       const CapabilitySpec& capabilities,
                                       mojom::ShellClientRequest* request) {
  CHECK(target_id.user_id() != mojom::kInheritUserID);
  mojom::ShellClientPtr shell_client;
  *request = GetProxy(&shell_client);
  Instance* instance =
      new Instance(std::move(shell_client), this, target_id, capabilities);
  DCHECK(identity_to_instance_.find(target_id) ==
         identity_to_instance_.end());
  identity_to_instance_[target_id] = instance;
  mojom::InstanceInfoPtr info = instance->CreateInstanceInfo();
  instance_listeners_.ForAllPtrs(
      [this, &info](mojom::InstanceListener* listener) {
        listener->InstanceCreated(info.Clone());
      });
  instance->InitializeClient();
  return instance;
}

void Shell::AddInstanceListener(mojom::InstanceListenerPtr listener) {
  // TODO(beng): filter instances provided by those visible to this client.
  Array<mojom::InstanceInfoPtr> instances;
  for (auto& instance : identity_to_instance_)
    instances.push_back(instance.second->CreateInstanceInfo());
  listener->SetExistingInstances(std::move(instances));

  instance_listeners_.AddInterfacePtr(std::move(listener));
}

void Shell::CreateShellClient(const Identity& source,
                              const Identity& shell_client_factory,
                              const std::string& name,
                              mojom::ShellClientRequest request) {
  mojom::ShellClientFactory* factory =
      GetShellClientFactory(shell_client_factory, source);
  factory->CreateShellClient(std::move(request), name);
}

mojom::ShellClientFactory* Shell::GetShellClientFactory(
    const Identity& shell_client_factory_identity,
    const Identity& source_identity) {
  auto it = shell_client_factories_.find(shell_client_factory_identity);
  if (it != shell_client_factories_.end())
    return it->second.get();

  mojom::ShellClientFactoryPtr factory;
  // TODO(beng): we should forward the original source identity!
  ConnectToInterface(this, source_identity, shell_client_factory_identity,
                     &factory);
  mojom::ShellClientFactory* factory_interface = factory.get();
  factory.set_connection_error_handler(
      base::Bind(&Shell::OnShellClientFactoryLost,
                 weak_ptr_factory_.GetWeakPtr(),
                 shell_client_factory_identity));
  shell_client_factories_[shell_client_factory_identity] = std::move(factory);
  return factory_interface;
}

void Shell::OnShellClientFactoryLost(const Identity& which) {
  // Remove the mapping.
  auto it = shell_client_factories_.find(which);
  DCHECK(it != shell_client_factories_.end());
  shell_client_factories_.erase(it);
}

void Shell::OnGotResolvedName(scoped_ptr<ConnectParams> params,
                              const String& resolved_name,
                              const String& resolved_instance,
                              mojom::CapabilitySpecPtr capabilities_ptr,
                              const String& file_url) {
  std::string instance_name = params->target().instance();
  if (instance_name == GetNamePath(params->target().name()) &&
      resolved_instance != GetNamePath(resolved_name)) {
    instance_name = resolved_instance;
  }
  Identity target(params->target().name(), params->target().user_id(),
                  instance_name);
  params->set_target(target);

  // It's possible that when this manifest request was issued, another one was
  // already in-progress and completed by the time this one did, and so the
  // requested application may already be running.
  if (ConnectToExistingInstance(&params))
    return;

  Identity source = params->source();
  // |capabilities_ptr| can be null when there is no manifest, e.g. for URL
  // types not resolvable by the resolver.
  CapabilitySpec capabilities = GetPermissiveCapabilities();
  if (!capabilities_ptr.is_null())
    capabilities = capabilities_ptr.To<CapabilitySpec>();

  mojom::ClientProcessConnectionPtr client_process_connection =
      params->TakeClientProcessConnection();
  mojom::ShellClientRequest request;
  Instance* instance = CreateInstance(target, capabilities, &request);
  instance->ConnectToClient(std::move(params));

  if (LoadWithLoader(target, &request))
    return;

  CHECK(!file_url.is_null() && !capabilities_ptr.is_null());

  if (target.name() != resolved_name) {
    // In cases where a package alias is resolved, we have to use the instance
    // from the original request rather than for the package itself, which will
    // always be the same.
    CreateShellClient(
        source, Identity(resolved_name, target.user_id(), instance_name),
        target.name(), std::move(request));
  } else {
    if (!client_process_connection.is_null()) {
      // The client already started a process for this instance, use it.
      instance->StartWithClientProcessConnection(
          std::move(request), std::move(client_process_connection));
    } else {
      // Otherwise we make our own process.
      instance->StartWithFilePath(std::move(request),
                                  util::UrlToFilePath(file_url.To<GURL>()));
    }
  }
}

bool Shell::LoadWithLoader(const Identity& target,
                           mojom::ShellClientRequest* request) {
  Loader* loader = GetLoaderForName(target.name());
  if (!loader)
    return false;
  loader->Load(target.name(), std::move(*request));
  return true;
}

Loader* Shell::GetLoaderForName(const std::string& name) {
  auto name_it = name_to_loader_.find(name);
  if (name_it != name_to_loader_.end())
    return name_it->second;
  return default_loader_.get();
}

void Shell::CleanupRunner(NativeRunner* runner) {
  for (auto it = native_runners_.begin(); it != native_runners_.end(); ++it) {
    if (it->get() == runner) {
      native_runners_.erase(it);
      return;
    }
  }
}

}  // namespace shell
}  // namespace mojo
