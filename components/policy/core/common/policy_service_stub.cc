// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_service_stub.h"


namespace policy {

PolicyServiceStub::PolicyServiceStub() {}

PolicyServiceStub::~PolicyServiceStub() {}

void PolicyServiceStub::AddObserver(PolicyDomain domain,
                                    Observer* observer) {}

void PolicyServiceStub::RemoveObserver(PolicyDomain domain,
                                       Observer* observer) {}

const PolicyMap& PolicyServiceStub::GetPolicies(
    const PolicyNamespace& ns) const {
  return kEmpty_;
}

bool PolicyServiceStub::IsInitializationComplete(PolicyDomain domain) const {
  return true;
}

void PolicyServiceStub::RefreshPolicies(const base::Closure& callback) {
  if (!callback.is_null())
    callback.Run();
}

}  // namespace policy
