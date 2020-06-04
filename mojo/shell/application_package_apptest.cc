// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/threading/simple_thread.h"
#include "mojo/public/cpp/bindings/weak_binding_set.h"
#include "mojo/shell/application_package_apptest.mojom.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/content_handler.mojom.h"

// Tests that multiple applications can be packaged in a single Mojo application
// implementing ContentHandler; that these applications can be specified by
// the package's manifest and are thus registered with the PackageManager.

namespace mojo {
namespace shell {
namespace {

using GetNameCallback =
    test::mojom::ApplicationPackageApptestService::GetNameCallback;

class ProvidedApplicationDelegate
    : public ShellClient,
      public InterfaceFactory<test::mojom::ApplicationPackageApptestService>,
      public test::mojom::ApplicationPackageApptestService,
      public base::SimpleThread {
 public:
  ProvidedApplicationDelegate(const std::string& name,
                              InterfaceRequest<mojom::ShellClient> request,
                              const Callback<void()>& destruct_callback)
      : base::SimpleThread(name),
        name_(name),
        request_(std::move(request)),
        destruct_callback_(destruct_callback) {
    Start();
  }
  ~ProvidedApplicationDelegate() override {
    Join();
    destruct_callback_.Run();
  }

 private:
  // mojo::ShellClient:
  void Initialize(Shell* shell, const std::string& url, uint32_t id) override {}
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<test::mojom::ApplicationPackageApptestService>(
        this);
    return true;
  }

  // InterfaceFactory<test::mojom::ApplicationPackageApptestService>:
  void Create(
      Connection* connection,
      InterfaceRequest<test::mojom::ApplicationPackageApptestService> request)
          override {
    bindings_.AddBinding(this, std::move(request));
  }

  // test::mojom::ApplicationPackageApptestService:
  void GetName(const GetNameCallback& callback) override {
    callback.Run(name_);
  }

  // base::SimpleThread:
  void Run() override {
    ApplicationRunner(this).Run(request_.PassMessagePipe().release().value(),
                                false);
    delete this;
  }

  const std::string name_;
  InterfaceRequest<mojom::ShellClient> request_;
  const Callback<void()> destruct_callback_;
  WeakBindingSet<test::mojom::ApplicationPackageApptestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ProvidedApplicationDelegate);
};

class ApplicationPackageApptestDelegate
    : public ShellClient,
      public InterfaceFactory<mojom::ContentHandler>,
      public InterfaceFactory<test::mojom::ApplicationPackageApptestService>,
      public mojom::ContentHandler,
      public test::mojom::ApplicationPackageApptestService {
 public:
  ApplicationPackageApptestDelegate() {}
  ~ApplicationPackageApptestDelegate() override {}

 private:
  // mojo::ShellClient:
  void Initialize(Shell* shell, const std::string& url, uint32_t id) override {}
  bool AcceptConnection(Connection* connection) override {
    connection->AddInterface<ContentHandler>(this);
    connection->AddInterface<test::mojom::ApplicationPackageApptestService>(
        this);
    return true;
  }

  // InterfaceFactory<mojom::ContentHandler>:
  void Create(Connection* connection,
              InterfaceRequest<mojom::ContentHandler> request) override {
    content_handler_bindings_.AddBinding(this, std::move(request));
  }

  // InterfaceFactory<test::mojom::ApplicationPackageApptestService>:
  void Create(Connection* connection,
              InterfaceRequest<test::mojom::ApplicationPackageApptestService>
                  request) override {
    bindings_.AddBinding(this, std::move(request));
  }

  // mojom::ContentHandler:
  void StartApplication(InterfaceRequest<mojom::ShellClient> request,
                        URLResponsePtr response,
                        const Callback<void()>& destruct_callback) override {
    const std::string url = response->url;
    if (url == "mojo://package_test_a/") {
      new ProvidedApplicationDelegate("A", std::move(request),
                                      destruct_callback);
    } else if (url == "mojo://package_test_b/") {
      new ProvidedApplicationDelegate("B", std::move(request),
                                      destruct_callback);
    }
  }

  // test::mojom::ApplicationPackageApptestService:
  void GetName(const GetNameCallback& callback) override {
    callback.Run("ROOT");
  }

  std::vector<scoped_ptr<ShellClient>> delegates_;
  WeakBindingSet<mojom::ContentHandler> content_handler_bindings_;
  WeakBindingSet<test::mojom::ApplicationPackageApptestService> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationPackageApptestDelegate);
};

void ReceiveName(std::string* out_name,
                 base::RunLoop* loop,
                 const String& name) {
  *out_name = name;
  loop->Quit();
}

}  // namespace

class ApplicationPackageApptest : public mojo::test::ApplicationTestBase {
 public:
  ApplicationPackageApptest() : delegate_(nullptr) {}
  ~ApplicationPackageApptest() override {}

 private:
  // test::ApplicationTestBase:
  ShellClient* GetShellClient() override {
    delegate_ = new ApplicationPackageApptestDelegate;
    return delegate_;
  }

  ApplicationPackageApptestDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ApplicationPackageApptest);
};

TEST_F(ApplicationPackageApptest, Basic) {
  {
    // We need to do this to force the shell to read the test app's manifest and
    // register aliases.
    test::mojom::ApplicationPackageApptestServicePtr root_service;
    shell()->ConnectToInterface("mojo:mojo_shell_apptests", &root_service);
    base::RunLoop run_loop;
    std::string root_name;
    root_service->GetName(base::Bind(&ReceiveName, &root_name, &run_loop));
    run_loop.Run();
  }

  {
    // Now subsequent connects to applications provided by the root app will be
    // resolved correctly.
    test::mojom::ApplicationPackageApptestServicePtr service_a;
    shell()->ConnectToInterface("mojo:package_test_a", &service_a);
    base::RunLoop run_loop;
    std::string a_name;
    service_a->GetName(base::Bind(&ReceiveName, &a_name, &run_loop));
    run_loop.Run();
    EXPECT_EQ("A", a_name);
  }

  {
    test::mojom::ApplicationPackageApptestServicePtr service_b;
    shell()->ConnectToInterface("mojo:package_test_b", &service_b);
    base::RunLoop run_loop;
    std::string b_name;
    service_b->GetName(base::Bind(&ReceiveName, &b_name, &run_loop));
    run_loop.Run();
    EXPECT_EQ("B", b_name);
  }
}

}  // namespace shell
}  // namespace mojo
