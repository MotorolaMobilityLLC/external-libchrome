// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/services/public/js/application", [
  "services/js/app_bridge",
  "mojo/services/public/js/service_provider",
  "mojo/services/public/js/shell",
], function(appBridge, serviceProvider, shell) {

  const ServiceProvider = serviceProvider.ServiceProvider;
  const Shell = shell.Shell;

  class Application {
    constructor(shellHandle, url) {
      this.url = url;
      this.serviceProviders = [];
      this.shellHandle_ = shellHandle;
      this.shell = new Shell(shellHandle, {
        initialize: this.initialize.bind(this),
        acceptConnection: this.doAcceptConnection.bind(this),
      });
    }

    initialize(args) {
    }

    doAcceptConnection(url, serviceProviderProxy) {
      var serviceProvider =  new ServiceProvider(serviceProviderProxy);
      this.serviceProviders.push(serviceProvider);
      this.acceptConnection(url, serviceProvider);
    }

    acceptConnection(url, serviceProvider) {
    }

    quit() {
      this.serviceProviders.forEach(function(sp) {
        sp.close();
      });
      this.shell.close();
      appBridge.quit();
    }
  }

  var exports = {};
  exports.Application = Application;
  return exports;
});
