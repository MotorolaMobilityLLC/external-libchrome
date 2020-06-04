// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/test_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// The end-to-end test exercises the asynchronos APIs in ObjectProxy and
// ExportedObject.
class EndToEndAsyncTest : public testing::Test {
 public:
  EndToEndAsyncTest() {
  }

  void SetUp() {
    // Make the main thread not to allow IO.
    base::ThreadRestrictions::SetIOAllowed(false);

    // Start the D-Bus thread.
    dbus_thread_.reset(new base::Thread("D-Bus Thread"));
    base::Thread::Options thread_options;
    thread_options.message_loop_type = MessageLoop::TYPE_IO;
    ASSERT_TRUE(dbus_thread_->StartWithOptions(thread_options));

    // Start the test service, using the D-Bus thread.
    dbus::TestService::Options options;
    options.dbus_thread = dbus_thread_.get();
    test_service_.reset(new dbus::TestService(options));
    ASSERT_TRUE(test_service_->StartService());
    ASSERT_TRUE(test_service_->WaitUntilServiceIsStarted());
    ASSERT_TRUE(test_service_->HasDBusThread());

    // Create the client, using the D-Bus thread.
    dbus::Bus::Options bus_options;
    bus_options.bus_type = dbus::Bus::SESSION;
    bus_options.connection_type = dbus::Bus::PRIVATE;
    bus_options.dbus_thread = dbus_thread_.get();
    bus_ = new dbus::Bus(bus_options);
    object_proxy_ = bus_->GetObjectProxy("org.chromium.TestService",
                                         "/org/chromium/TestObject");
    ASSERT_TRUE(bus_->HasDBusThread());

    // Connect to the "Test" signal from the remote object.
    object_proxy_->ConnectToSignal(
        "org.chromium.TestInterface",
        "Test",
        base::Bind(&EndToEndAsyncTest::OnTestSignal,
                   base::Unretained(this)),
        base::Bind(&EndToEndAsyncTest::OnConnected,
                   base::Unretained(this)));
    // Wait until the object proxy is connected to the signal.
    message_loop_.Run();
  }

  void TearDown() {
    bus_->Shutdown(base::Bind(&EndToEndAsyncTest::OnShutdown,
                              base::Unretained(this)));
    // Wait until the bus is shutdown. OnShutdown() will be called in
    // mesage_loop_.
    message_loop_.Run();

    // Shut down the service.
    test_service_->Shutdown();
    ASSERT_TRUE(test_service_->WaitUntilServiceIsShutdown());

    // Reset to the default.
    base::ThreadRestrictions::SetIOAllowed(true);

    // Stopping a thread is considred an IO operation, so do this after
    // allowing IO.
    test_service_->Stop();
  }

 protected:
  // Calls the method asynchronosly. OnResponse() will be called once the
  // response is received.
  void CallMethod(dbus::MethodCall* method_call,
                  int timeout_ms) {
    object_proxy_->CallMethod(method_call,
                              timeout_ms,
                              base::Bind(&EndToEndAsyncTest::OnResponse,
                                         base::Unretained(this)));
  }

  // Wait for the give number of responses.
  void WaitForResponses(size_t num_responses) {
    while (response_strings_.size() < num_responses) {
      message_loop_.Run();
    }
  }

  // Called when the response is received.
  void OnResponse(dbus::Response* response) {
    // |response| will be deleted on exit of the function. Copy the
    // payload to |response_strings_|.
    if (response) {
      dbus::MessageReader reader(response);
      std::string response_string;
      ASSERT_TRUE(reader.PopString(&response_string));
      response_strings_.push_back(response_string);
    } else {
      response_strings_.push_back("");
    }
    message_loop_.Quit();
  };

  // Called when the shutdown is complete.
  void OnShutdown() {
    message_loop_.Quit();
  }

  // Called when the "Test" signal is received, in the main thread.
  // Copy the string payload to |test_signal_string_|.
  void OnTestSignal(dbus::Signal* signal) {
    dbus::MessageReader reader(signal);
    ASSERT_TRUE(reader.PopString(&test_signal_string_));
    message_loop_.Quit();
  }

  // Called when connected to the signal.
  void OnConnected(const std::string& interface_name,
                   const std::string& signal_name,
                   bool success) {
    ASSERT_TRUE(success);
    message_loop_.Quit();
  }

  // Wait for the hey signal to be received.
  void WaitForTestSignal() {
    // OnTestSignal() will quit the message loop.
    message_loop_.Run();
  }

  MessageLoop message_loop_;
  std::vector<std::string> response_strings_;
  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* object_proxy_;
  scoped_ptr<dbus::TestService> test_service_;
  // Text message from "Test" signal.
  std::string test_signal_string_;
};

TEST_F(EndToEndAsyncTest, Echo) {
  const char* kHello = "hello";

  // Create the method call.
  dbus::MethodCall method_call("org.chromium.TestInterface", "Echo");
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kHello);

  // Call the method.
  const int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;
  CallMethod(&method_call, timeout_ms);

  // Check the response.
  WaitForResponses(1);
  EXPECT_EQ(kHello, response_strings_[0]);
}

// Call Echo method three times.
TEST_F(EndToEndAsyncTest, EchoThreeTimes) {
  const char* kMessages[] = { "foo", "bar", "baz" };

  for (size_t i = 0; i < arraysize(kMessages); ++i) {
    // Create the method call.
    dbus::MethodCall method_call("org.chromium.TestInterface", "Echo");
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(kMessages[i]);

    // Call the method.
    const int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;
    CallMethod(&method_call, timeout_ms);
  }

  // Check the responses.
  WaitForResponses(3);
  // Sort as the order of the returned messages is not deterministic.
  std::sort(response_strings_.begin(), response_strings_.end());
  EXPECT_EQ("bar", response_strings_[0]);
  EXPECT_EQ("baz", response_strings_[1]);
  EXPECT_EQ("foo", response_strings_[2]);
}

TEST_F(EndToEndAsyncTest, Timeout) {
  const char* kHello = "hello";

  // Create the method call.
  dbus::MethodCall method_call("org.chromium.TestInterface", "SlowEcho");
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kHello);

  // Call the method with timeout of 0ms.
  const int timeout_ms = 0;
  CallMethod(&method_call, timeout_ms);
  WaitForResponses(1);

  // Should fail because of timeout.
  ASSERT_EQ("", response_strings_[0]);
}

TEST_F(EndToEndAsyncTest, NonexistentMethod) {
  dbus::MethodCall method_call("org.chromium.TestInterface", "Nonexistent");

  const int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;
  CallMethod(&method_call, timeout_ms);
  WaitForResponses(1);

  // Should fail because the method is nonexistent.
  ASSERT_EQ("", response_strings_[0]);
}

TEST_F(EndToEndAsyncTest, BrokenMethod) {
  dbus::MethodCall method_call("org.chromium.TestInterface", "BrokenMethod");

  const int timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;
  CallMethod(&method_call, timeout_ms);
  WaitForResponses(1);

  // Should fail because the method is broken.
  ASSERT_EQ("", response_strings_[0]);
}

TEST_F(EndToEndAsyncTest, TestSignal) {
  const char kMessage[] = "hello, world";
  // Send the test signal from the exported object.
  test_service_->SendTestSignal(kMessage);
  // Receive the signal with the object proxy. The signal is handeled in
  // EndToEndAsyncTest::OnTestSignal() in the main thread.
  WaitForTestSignal();
  ASSERT_EQ(kMessage, test_signal_string_);
}
