// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/service_provider", [
  "mojo/public/js/bindings",
  "mojo/public/interfaces/application/service_provider.mojom",
  "mojo/public/js/connection",
], function(bindings, spMojom, connection) {

  const ProxyBindings = bindings.ProxyBindings;
  const StubBindings = bindings.StubBindings;
  const ServiceProviderInterface = spMojom.ServiceProvider;

  function checkServiceProvider(sp) {
    if (!sp.providers_)
      throw new Error("Service was closed");
  }

  class ServiceProvider {
    constructor(servicesRequest, exposedServicesProxy) {
      this.proxy = exposedServicesProxy;
      this.providers_ = new Map(); // serviceName => see provideService() below
      this.pendingRequests_ = new Map(); // serviceName => serviceHandle
      if (servicesRequest)
        StubBindings(servicesRequest).delegate = this;
    }

    // Incoming requests
    connectToService(serviceName, serviceHandle) {
      if (!this.providers_) // We're closed.
        return;

      var provider = this.providers_.get(serviceName);
      if (!provider) {
        this.pendingRequests_.set(serviceName, serviceHandle);
        return;
      }

      var stub = connection.bindHandleToStub(serviceHandle, provider.service);
      StubBindings(stub).delegate = new provider.factory();
      provider.connections.push(StubBindings(stub).connection);
    }

    provideService(service, factory) {
      checkServiceProvider(this);

      var provider = {
        service: service, // A JS bindings interface object.
        factory: factory, // factory(clientProxy) => interface implemntation
        connections: [],
      };
      this.providers_.set(service.name, provider);

      if (this.pendingRequests_.has(service.name)) {
        this.connectToService(service.name, pendingRequests_.get(service.name));
        pendingRequests_.delete(service.name);
      }
      return this;
    }

    // Outgoing requests
    requestService(interfaceObject, clientImpl) {
      checkServiceProvider(this);
      if (!interfaceObject.name)
        throw new Error("Invalid service parameter");
      if (!clientImpl && interfaceObject.client)
        throw new Error("Client implementation must be provided");

      var serviceProxy;
      var serviceHandle = connection.bindProxy(
          function(sp) {serviceProxy = sp;}, interfaceObject);
      this.proxy.connectToService(interfaceObject.name, serviceHandle);
      return serviceProxy;
    };

    close() {
      this.providers_ = null;
      this.pendingRequests_ = null;
    }
  }

  var exports = {};
  exports.ServiceProvider = ServiceProvider;
  return exports;
});
