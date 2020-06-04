// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "dbus/test_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// The test for sender verification in ObjectProxy.
class SignalSenderVerificationTest : public testing::Test {
 public:
  SignalSenderVerificationTest() {
  }

  virtual void SetUp() {
    base::StatisticsRecorder::Initialize();

    // Make the main thread not to allow IO.
    base::ThreadRestrictions::SetIOAllowed(false);

    // Start the D-Bus thread.
    dbus_thread_.reset(new base::Thread("D-Bus Thread"));
    base::Thread::Options thread_options;
    thread_options.message_loop_type = MessageLoop::TYPE_IO;
    ASSERT_TRUE(dbus_thread_->StartWithOptions(thread_options));

    // Start the test service, using the D-Bus thread.
    dbus::TestService::Options options;
    options.dbus_thread_message_loop_proxy = dbus_thread_->message_loop_proxy();
    test_service_.reset(new dbus::TestService(options));
    ASSERT_TRUE(test_service_->StartService());
    ASSERT_TRUE(test_service_->WaitUntilServiceIsStarted());
    ASSERT_TRUE(test_service_->HasDBusThread());

    // Same setup for the second TestService. This service should not have the
    // ownership of the name at this point.
    test_service2_.reset(new dbus::TestService(options));
    ASSERT_TRUE(test_service2_->StartService());
    ASSERT_TRUE(test_service2_->WaitUntilServiceIsStarted());
    ASSERT_TRUE(test_service2_->HasDBusThread());

    // Create the client, using the D-Bus thread.
    dbus::Bus::Options bus_options;
    bus_options.bus_type = dbus::Bus::SESSION;
    bus_options.connection_type = dbus::Bus::PRIVATE;
    bus_options.dbus_thread_message_loop_proxy =
        dbus_thread_->message_loop_proxy();
    bus_ = new dbus::Bus(bus_options);
    object_proxy_ = bus_->GetObjectProxy(
        "org.chromium.TestService",
        dbus::ObjectPath("/org/chromium/TestObject"));
    ASSERT_TRUE(bus_->HasDBusThread());

    // Connect to the "Test" signal of "org.chromium.TestInterface" from
    // the remote object.
    object_proxy_->ConnectToSignal(
        "org.chromium.TestInterface",
        "Test",
        base::Bind(&SignalSenderVerificationTest::OnTestSignal,
                   base::Unretained(this)),
        base::Bind(&SignalSenderVerificationTest::OnConnected,
                   base::Unretained(this)));
    // Wait until the object proxy is connected to the signal.
    message_loop_.Run();
  }

  virtual void TearDown() {
    bus_->ShutdownOnDBusThreadAndBlock();

    // Shut down the service.
    test_service_->ShutdownAndBlock();
    test_service2_->ShutdownAndBlock();

    // Reset to the default.
    base::ThreadRestrictions::SetIOAllowed(true);

    // Stopping a thread is considered an IO operation, so do this after
    // allowing IO.
    test_service_->Stop();
    test_service2_->Stop();
  }

 protected:

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
  scoped_ptr<base::Thread> dbus_thread_;
  scoped_refptr<dbus::Bus> bus_;
  dbus::ObjectProxy* object_proxy_;
  scoped_ptr<dbus::TestService> test_service_;
  scoped_ptr<dbus::TestService> test_service2_;
  // Text message from "Test" signal.
  std::string test_signal_string_;
};

TEST_F(SignalSenderVerificationTest, TestSignalAccepted) {
  const char kMessage[] = "hello, world";
  // Send the test signal from the exported object.
  test_service_->SendTestSignal(kMessage);
  // Receive the signal with the object proxy. The signal is handled in
  // SignalSenderVerificationTest::OnTestSignal() in the main thread.
  WaitForTestSignal();
  ASSERT_EQ(kMessage, test_signal_string_);
}

TEST_F(SignalSenderVerificationTest, TestSignalRejected) {
  // To make sure the histogram instance is created.
  UMA_HISTOGRAM_COUNTS("DBus.RejectedSignalCount", 0);
  base::Histogram* reject_signal_histogram =
        base::StatisticsRecorder::FindHistogram("DBus.RejectedSignalCount");
  scoped_ptr<base::HistogramSamples> samples1(
      reject_signal_histogram->SnapshotSamples());

  const char kNewMessage[] = "hello, new world";
  test_service2_->SendTestSignal(kNewMessage);

  // This test tests that our callback is NOT called by the ObjectProxy.
  // Sleep to have message delivered to the client via the D-Bus service.
  base::PlatformThread::Sleep(TestTimeouts::action_timeout());

  scoped_ptr<base::HistogramSamples> samples2(
      reject_signal_histogram->SnapshotSamples());

  ASSERT_EQ("", test_signal_string_);
  EXPECT_EQ(samples1->TotalCount() + 1, samples2->TotalCount());
}

TEST_F(SignalSenderVerificationTest, DISABLED_TestOwnerChanged) {
  const char kMessage[] = "hello, world";

  // Send the test signal from the exported object.
  test_service_->SendTestSignal(kMessage);
  // Receive the signal with the object proxy. The signal is handled in
  // SignalSenderVerificationTest::OnTestSignal() in the main thread.
  WaitForTestSignal();
  ASSERT_EQ(kMessage, test_signal_string_);

  // Release and aquire the name ownership.
  test_service_->ShutdownAndBlock();
  test_service2_->RequestOwnership();

  // Now the second service owns the name.
  const char kNewMessage[] = "hello, new world";

  test_service2_->SendTestSignal(kNewMessage);
  WaitForTestSignal();
  ASSERT_EQ(kNewMessage, test_signal_string_);
}
