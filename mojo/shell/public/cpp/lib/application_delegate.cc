// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/public/cpp/application_delegate.h"

namespace mojo {

ApplicationDelegate::ApplicationDelegate() {
}
ApplicationDelegate::~ApplicationDelegate() {
}

void ApplicationDelegate::Initialize(ApplicationImpl* app) {
}

bool ApplicationDelegate::AcceptConnection(ApplicationConnection* connection) {
  return true;
}

bool ApplicationDelegate::ShellConnectionLost() {
  return true;
}

void ApplicationDelegate::Quit() {
}

}  // namespace mojo
