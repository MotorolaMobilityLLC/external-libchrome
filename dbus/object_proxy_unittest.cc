// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "dbus/bus.h"
#include "dbus/object_proxy.h"
#include "dbus/test_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dbus {
namespace {

class ObjectProxyTest : public testing::Test {
 protected:
  void SetUp() override {
    Bus::Options bus_options;
    bus_options.bus_type = Bus::SESSION;
    bus_options.connection_type = Bus::PRIVATE;
    bus_ = new Bus(bus_options);
  }

  void TearDown() override { bus_->ShutdownAndBlock(); }

  base::MessageLoopForIO message_loop_;
  scoped_refptr<Bus> bus_;
};

// Used as a WaitForServiceToBeAvailableCallback.
void OnServiceIsAvailable(scoped_ptr<base::RunLoop>* run_loop,
                          bool service_is_available) {
  EXPECT_TRUE(service_is_available);
  ASSERT_TRUE(*run_loop);
  (*run_loop)->Quit();
}

TEST_F(ObjectProxyTest, WaitForServiceToBeAvailable) {
  scoped_ptr<base::RunLoop> run_loop;

  TestService::Options options;
  TestService test_service(options);

  // Callback is not yet called because the service is not available.
  ObjectProxy* object_proxy = bus_->GetObjectProxy(
      test_service.service_name(), ObjectPath("/org/chromium/TestObject"));
  object_proxy->WaitForServiceToBeAvailable(
      base::Bind(&OnServiceIsAvailable, &run_loop));
  base::RunLoop().RunUntilIdle();

  // Start the service.
  ASSERT_TRUE(test_service.StartService());
  ASSERT_TRUE(test_service.WaitUntilServiceIsStarted());
  ASSERT_TRUE(test_service.has_ownership());

  // Callback is called beacuse the service became available.
  run_loop.reset(new base::RunLoop);
  run_loop->Run();

  // Callback is called because the service is already available.
  run_loop.reset(new base::RunLoop);
  object_proxy->WaitForServiceToBeAvailable(
      base::Bind(&OnServiceIsAvailable, &run_loop));
  run_loop->Run();

  // Shut down the service.
  test_service.ShutdownAndBlock();
  test_service.Stop();
}

}  // namespace
}  // namespace dbus
