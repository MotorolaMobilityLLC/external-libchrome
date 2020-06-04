// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings/connection", [
  "mojo/public/js/bindings/connector",
  "mojo/public/js/bindings/router",
], function(connector, router) {

  function Connection(
      handle, localFactory, remoteFactory, routerFactory, connectorFactory) {
    if (routerFactory === undefined)
      routerFactory = router.Router;
    this.router_ = new routerFactory(handle, connectorFactory);
    this.remote = new remoteFactory(this.router_);
    this.local = new localFactory(this.remote);
    this.router_.setIncomingReceiver(this.local);

    var validateRequest = localFactory.prototype.validator;
    var validateResponse = remoteFactory.prototype.validator;
    var payloadValidators = [];
    if (validateRequest)
      payloadValidators.push(validateRequest);
    if (validateResponse)
      payloadValidators.push(validateResponse);
    this.router_.setPayloadValidators(payloadValidators);
  }

  Connection.prototype.close = function() {
    this.router_.close();
    this.router_ = null;
    this.local = null;
    this.remote = null;
  };

  Connection.prototype.encounteredError = function() {
    return this.router_.encounteredError();
  };

  // The TestConnection subclass is only intended to be used in unit tests.

  function TestConnection(handle, localFactory, remoteFactory) {
    Connection.call(this,
                    handle,
                    localFactory,
                    remoteFactory,
                    router.TestRouter,
                    connector.TestConnector);
  }

  TestConnection.prototype = Object.create(Connection.prototype);

  var exports = {};
  exports.Connection = Connection;
  exports.TestConnection = TestConnection;
  return exports;
});
