// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "console",
    "mojo/public/bindings/js/test/hexdump",
    "gin/test/expect",
    // TODO(abarth): We shouldn't need to depend on codec, but there seems to
    // be a bug in the module loading system whereby this test doesn't run if
    // we don't import codec here.
    "mojo/public/bindings/js/codec",
    "mojom/sample_service"
  ], function(console, hexdump, expect, codec, sample) {

  var global = this;

  // Set this variable to true to print the binary message in hex.
  var dumpMessageAsHex = false;

  function makeFoo() {
    var bar = new sample.Bar();
    bar.alpha = 20;
    bar.beta = 40;
    bar.gamma = 60;

    var extra_bars = new Array(3);
    for (var i = 0; i < extra_bars.length; ++i) {
      var base = i * 100;
      extra_bars[i] = new sample.Bar();
      extra_bars[i].alpha = base;
      extra_bars[i].beta = base + 20;
      extra_bars[i].gamma = base + 40;
    }

    var data = new Array(10);
    for (var i = 0; i < data.length; ++i) {
      data[i] = data.length - i;
    }

    var files = new Array(4);
    for (var i = 0; i < files.length; ++i) {
      files[i] = 0xFFFF - i;
    }

    var foo = new sample.Foo();
    foo.name = "foopy";
    foo.x = 1;
    foo.y = 2;
    foo.a = false;
    foo.b = true;
    foo.c = false;
    foo.bar = bar;
    foo.extra_bars = extra_bars;
    foo.data = data;
    foo.files = files;
    return foo;
  }

  // Check that the given |Foo| is identical to the one made by |MakeFoo()|.
  function checkFoo(foo) {
    expect(foo.name).toBe("foopy");
    expect(foo.x).toBe(1);
    expect(foo.y).toBe(2);
    expect(foo.a).toBeFalsy();
    expect(foo.b).toBeTruthy();
    expect(foo.c).toBeFalsy();
    expect(foo.bar.alpha).toBe(20);
    expect(foo.bar.beta).toBe(40);
    expect(foo.bar.gamma).toBe(60);

    expect(foo.extra_bars.length).toBe(3);
    for (var i = 0; i < foo.extra_bars.length; ++i) {
      var base = i * 100;
      expect(foo.extra_bars[i].alpha).toBe(base);
      expect(foo.extra_bars[i].beta).toBe(base + 20);
      expect(foo.extra_bars[i].gamma).toBe(base + 40);
    }

    expect(foo.data.length).toBe(10);
    for (var i = 0; i < foo.data.length; ++i)
      expect(foo.data[i]).toBe(foo.data.length - i);

    expect(foo.files.length).toBe(4);
    for (var i = 0; i < foo.files.length; ++i)
      expect(foo.files[i]).toBe(0xFFFF - i);
  }

  function ServiceImpl() {
  }

  ServiceImpl.prototype = Object.create(sample.ServiceStub.prototype);

  ServiceImpl.prototype.frobinate = function(foo, baz, port) {
    checkFoo(foo);
    expect(baz).toBeTruthy();
    expect(port).toBe(10);
    global.result = "PASS";
  };

  function SimpleMessageReceiver() {
  }

  SimpleMessageReceiver.prototype.accept = function(message) {
    if (dumpMessageAsHex)
      console.log(hexdump.dumpArray(message.memory));
    // Imagine some IPC happened here.
    var serviceImpl = new ServiceImpl();
    serviceImpl.accept(message);
  };

  var receiver = new SimpleMessageReceiver();
  var serviceProxy = new sample.ServiceProxy(receiver);

  var foo = makeFoo();
  checkFoo(foo);

  var port = 10;
  serviceProxy.frobinate(foo, true, port);
});
