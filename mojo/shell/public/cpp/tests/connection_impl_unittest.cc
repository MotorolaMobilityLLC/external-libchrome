// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/lib/connection_impl.h"

#include "base/memory/scoped_ptr.h"
#include "mojo/shell/public/cpp/service_connector.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace internal {
namespace {

class TestConnector : public ServiceConnector {
 public:
  explicit TestConnector(int* delete_count) : delete_count_(delete_count) {}
  ~TestConnector() override { (*delete_count_)++; }
  void ConnectToService(Connection* connection,
                        const std::string& interface_name,
                        ScopedMessagePipeHandle client_handle) override {}

 private:
  int* delete_count_;
};

TEST(ConnectionImplTest, Ownership) {
  int delete_count = 0;

  // Destruction.
  {
    ConnectionImpl connection;
    ConnectionImpl::TestApi test_api(&connection);
    test_api.SetServiceConnectorForName(new TestConnector(&delete_count),
                                        "TC1");
  }
  EXPECT_EQ(1, delete_count);

  // Removal.
  {
    scoped_ptr<ConnectionImpl> connection(new ConnectionImpl);
    ServiceConnector* c = new TestConnector(&delete_count);
    ConnectionImpl::TestApi test_api(connection.get());
    test_api.SetServiceConnectorForName(c, "TC1");
    test_api.RemoveServiceConnectorForName("TC1");
    connection.reset();
    EXPECT_EQ(2, delete_count);
  }

  // Multiple.
  {
    ConnectionImpl connection;
    ConnectionImpl::TestApi test_api(&connection);
    test_api.SetServiceConnectorForName(new TestConnector(&delete_count),
                                        "TC1");
    test_api.SetServiceConnectorForName(new TestConnector(&delete_count),
                                        "TC2");
  }
  EXPECT_EQ(4, delete_count);

  // Re-addition.
  {
    ConnectionImpl connection;
    ConnectionImpl::TestApi test_api(&connection);
    test_api.SetServiceConnectorForName(new TestConnector(&delete_count),
                                        "TC1");
    test_api.SetServiceConnectorForName(new TestConnector(&delete_count),
                                        "TC1");
    EXPECT_EQ(5, delete_count);
  }
  EXPECT_EQ(6, delete_count);
}

}  // namespace
}  // namespace internal
}  // namespace mojo
