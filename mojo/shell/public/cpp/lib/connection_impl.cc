// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/lib/connection_impl.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/interface_binder.h"

namespace mojo {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, public:

ConnectionImpl::ConnectionImpl(
    const std::string& connection_name,
    const std::string& remote_name,
    uint32_t remote_id,
    const std::string& remote_user_id,
    shell::mojom::InterfaceProviderPtr remote_interfaces,
    shell::mojom::InterfaceProviderRequest local_interfaces,
    const std::set<std::string>& allowed_interfaces)
    : connection_name_(connection_name),
      remote_name_(remote_name),
      remote_id_(remote_id),
      connection_completed_(
          remote_id != shell::mojom::Connector::kInvalidApplicationID),
      remote_user_id_(remote_user_id),
      local_registry_(std::move(local_interfaces), this),
      remote_interfaces_(std::move(remote_interfaces)),
      allowed_interfaces_(allowed_interfaces),
      allow_all_interfaces_(allowed_interfaces_.size() == 1 &&
                            allowed_interfaces_.count("*") == 1),
      weak_factory_(this) {}

ConnectionImpl::ConnectionImpl()
    : local_registry_(shell::mojom::InterfaceProviderRequest(), this),
      allow_all_interfaces_(true),
      weak_factory_(this) {}

ConnectionImpl::~ConnectionImpl() {}

shell::mojom::Connector::ConnectCallback ConnectionImpl::GetConnectCallback() {
  return base::Bind(&ConnectionImpl::OnConnectionCompleted,
                    weak_factory_.GetWeakPtr());
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, Connection implementation:

const std::string& ConnectionImpl::GetConnectionName() {
  return connection_name_;
}

const std::string& ConnectionImpl::GetRemoteApplicationName() {
  return remote_name_;
}

const std::string& ConnectionImpl::GetRemoteUserID() const {
  return remote_user_id_;
}

void ConnectionImpl::SetConnectionLostClosure(const Closure& handler) {
  remote_interfaces_.set_connection_error_handler(handler);
}

bool ConnectionImpl::GetConnectionResult(
    shell::mojom::ConnectResult* result) const {
  if (!connection_completed_)
    return false;
  *result = result_;
  return true;
}


bool ConnectionImpl::GetRemoteApplicationID(uint32_t* remote_id) const {
  if (!connection_completed_)
    return false;

  *remote_id = remote_id_;
  return true;
}

void ConnectionImpl::AddConnectionCompletedClosure(const Closure& callback) {
  if (connection_completed_) {
    callback.Run();
    return;
  }
  connection_completed_callbacks_.push_back(callback);
}

bool ConnectionImpl::AllowsInterface(const std::string& interface_name) const {
  return allow_all_interfaces_ || allowed_interfaces_.count(interface_name);
}

shell::mojom::InterfaceProvider* ConnectionImpl::GetRemoteInterfaces() {
  return remote_interfaces_.get();
}

InterfaceRegistry* ConnectionImpl::GetLocalRegistry() {
  return &local_registry_;
}

base::WeakPtr<Connection> ConnectionImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

////////////////////////////////////////////////////////////////////////////////
// ConnectionImpl, private:

void ConnectionImpl::OnConnectionCompleted(shell::mojom::ConnectResult result,
                                           const std::string& target_user_id,
                                           uint32_t target_application_id) {
  DCHECK(!connection_completed_);
  connection_completed_ = true;

  result_ = result;
  remote_id_ = target_application_id;
  remote_user_id_= target_user_id;
  std::vector<Closure> callbacks;
  callbacks.swap(connection_completed_callbacks_);
  for (auto callback : callbacks)
    callback.Run();
}

}  // namespace internal
}  // namespace mojo
