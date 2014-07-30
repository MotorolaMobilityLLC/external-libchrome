// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/context.h"

#include "build/build_config.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_vector.h"
#include "mojo/embedder/embedder.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/service_manager/background_service_loader.h"
#include "mojo/service_manager/service_loader.h"
#include "mojo/service_manager/service_manager.h"
#include "mojo/services/native_viewport/native_viewport_service.h"
#include "mojo/shell/dynamic_service_loader.h"
#include "mojo/shell/in_process_dynamic_service_runner.h"
#include "mojo/shell/out_of_process_dynamic_service_runner.h"
#include "mojo/shell/switches.h"
#include "mojo/shell/ui_service_loader_android.h"
#include "mojo/spy/spy.h"

#if defined(OS_LINUX)
#include "mojo/shell/dbus_service_loader_linux.h"
#endif  // defined(OS_LINUX)

#if defined(OS_ANDROID)
#include "mojo/shell/network_service_loader.h"
#endif  // defined(OS_ANDROID)

#if defined(USE_AURA)
#include "mojo/shell/view_manager_loader.h"
#endif

namespace mojo {
namespace shell {
namespace {

// These mojo: URLs are loaded directly from the local filesystem. They
// correspond to shared libraries bundled alongside the mojo_shell.
const char* kLocalMojoURLs[] = {
  "mojo:mojo_network_service",
};

// Used to ensure we only init once.
class Setup {
 public:
  Setup() {
    embedder::Init();
  }

  ~Setup() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(Setup);
};

static base::LazyInstance<Setup>::Leaky setup = LAZY_INSTANCE_INITIALIZER;

}  // namespace

class Context::NativeViewportServiceLoader : public ServiceLoader {
 public:
  NativeViewportServiceLoader() {}
  virtual ~NativeViewportServiceLoader() {}

 private:
  virtual void LoadService(ServiceManager* manager,
                           const GURL& url,
                           ScopedMessagePipeHandle shell_handle) OVERRIDE {
    app_.reset(services::CreateNativeViewportService(shell_handle.Pass()));
  }

  virtual void OnServiceError(ServiceManager* manager,
                              const GURL& url) OVERRIDE {
  }

  scoped_ptr<ApplicationImpl> app_;
  DISALLOW_COPY_AND_ASSIGN(NativeViewportServiceLoader);
};

Context::Context() {
  DCHECK(!base::MessageLoop::current());
}

void Context::Init() {
  setup.Get();
  task_runners_.reset(
      new TaskRunners(base::MessageLoop::current()->message_loop_proxy()));

  for (size_t i = 0; i < arraysize(kLocalMojoURLs); ++i)
    mojo_url_resolver_.AddLocalFileMapping(GURL(kLocalMojoURLs[i]));

  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  scoped_ptr<DynamicServiceRunnerFactory> runner_factory;
  if (cmdline->HasSwitch(switches::kEnableMultiprocess))
    runner_factory.reset(new OutOfProcessDynamicServiceRunnerFactory());
  else
    runner_factory.reset(new InProcessDynamicServiceRunnerFactory());

  service_manager_.set_default_loader(
      scoped_ptr<ServiceLoader>(
          new DynamicServiceLoader(this, runner_factory.Pass())));
  // The native viewport service synchronously waits for certain messages. If we
  // don't run it on its own thread we can easily deadlock. Long term native
  // viewport should run its own process so that this isn't an issue.
#if defined(OS_ANDROID)
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(
          new UIServiceLoader(
              scoped_ptr<ServiceLoader>(new NativeViewportServiceLoader()),
              this)),
      GURL("mojo:mojo_native_viewport_service"));
#else
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(
          new BackgroundServiceLoader(
              scoped_ptr<ServiceLoader>(new NativeViewportServiceLoader()),
              "native_viewport",
              base::MessageLoop::TYPE_UI)),
      GURL("mojo:mojo_native_viewport_service"));
#endif
#if defined(USE_AURA)
  // TODO(sky): need a better way to find this. It shouldn't be linked in.
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(new ViewManagerLoader()),
      GURL("mojo:mojo_view_manager"));
#endif

#if defined(OS_LINUX)
  service_manager_.SetLoaderForScheme(
      scoped_ptr<ServiceLoader>(new DBusServiceLoader(this)),
      "dbus");
#endif  // defined(OS_LINUX)

  if (cmdline->HasSwitch(switches::kSpy)) {
    spy_.reset(new mojo::Spy(&service_manager_,
                             cmdline->GetSwitchValueASCII(switches::kSpy)));
  }

#if defined(OS_ANDROID)
  // On android, the network service is bundled with the shell because the
  // network stack depends on the android runtime.
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(
          new BackgroundServiceLoader(
              scoped_ptr<ServiceLoader>(new NetworkServiceLoader()),
              "network_service",
              base::MessageLoop::TYPE_IO)),
      GURL("mojo:mojo_network_service"));
#endif
}

void Context::Shutdown() {
  // mojo_view_manager uses native_viewport. Destroy mojo_view_manager first so
  // that there aren't shutdown ordering issues. Once native viewport service is
  // moved into its own process this can likely be nuked.
#if defined(USE_AURA)
  service_manager_.SetLoaderForURL(
      scoped_ptr<ServiceLoader>(),
      GURL("mojo:mojo_view_manager"));
#endif
  service_manager_.set_default_loader(scoped_ptr<ServiceLoader>());
  service_manager_.TerminateShellConnections();
}

Context::~Context() {
  DCHECK(!base::MessageLoop::current());
  Shutdown();
}

}  // namespace shell
}  // namespace mojo
