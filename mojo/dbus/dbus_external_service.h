// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "mojo/embedder/channel_init.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/interface_factory.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/external_service.mojom.h"

namespace mojo {
const char kMojoDBusImplPath[] = "/org/chromium/MojoImpl";
const char kMojoDBusInterface[] = "org.chromium.Mojo";
const char kMojoDBusConnectMethod[] = "ConnectChannel";

class DBusExternalServiceBase {
 public:
  explicit DBusExternalServiceBase(const std::string& service_name);
  virtual ~DBusExternalServiceBase();

  void Start();

 protected:
  // TODO(cmasone): Enable multiple peers to connect/disconnect
  virtual void Connect(ScopedMessagePipeHandle client_handle) = 0;
  virtual void Disconnect() = 0;

 private:
  // Implementation of org.chromium.Mojo.ConnectChannel, exported over DBus.
  // Takes a file descriptor and uses it to create a MessagePipe that is then
  // hooked to an implementation of ExternalService.
  void ConnectChannel(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender sender);

  void ExportMethods();
  void InitializeDBus();
  void TakeDBusServiceOwnership();

  const std::string service_name_;
  scoped_refptr<dbus::Bus> bus_;
  dbus::ExportedObject* exported_object_;  // Owned by bus_;
  scoped_ptr<embedder::ChannelInit> channel_init_;
  DISALLOW_COPY_AND_ASSIGN(DBusExternalServiceBase);
};

template <class ServiceImpl>
class DBusExternalService
    : public DBusExternalServiceBase,
      public ApplicationDelegate,
      public InterfaceFactory<typename ServiceImpl::ImplementedInterface> {
 public:
  explicit DBusExternalService(const std::string& service_name)
      : DBusExternalServiceBase(service_name) {
  }
  virtual ~DBusExternalService() {}

  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  virtual void Create(
      ApplicationConnection* connection,
      InterfaceRequest<typename ServiceImpl::ImplementedInterface> request)
      override {
    BindToRequest(new ServiceImpl, &request);
  }

 protected:
  virtual void Connect(ScopedMessagePipeHandle client_handle) override {
    external_service_.reset(BindToPipe(new Impl(this), client_handle.Pass()));
  }

  virtual void Disconnect() override {
    external_service_.reset();
  }

 private:
  class Impl : public InterfaceImpl<ExternalService> {
   public:
    explicit Impl(DBusExternalService* service) : service_(service) {
    }
    virtual void OnConnectionError() override {
      service_->Disconnect();
    }
    virtual void Activate(ScopedMessagePipeHandle service_provider_handle)
        override {
      app_.reset(new ApplicationImpl(service_, service_provider_handle.Pass()));
    }
   private:
    DBusExternalService* service_;
    scoped_ptr<Application> app_;
  };

  scoped_ptr<Impl> external_service_;
};

}  // namespace mojo
