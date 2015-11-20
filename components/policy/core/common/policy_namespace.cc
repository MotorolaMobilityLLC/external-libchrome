// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_namespace.h"

namespace policy {

PolicyNamespace::PolicyNamespace() {}

PolicyNamespace::PolicyNamespace(PolicyDomain domain,
                                 const std::string& component_id)
    : domain(domain),
      component_id(component_id) {}

PolicyNamespace::PolicyNamespace(const PolicyNamespace& other)
    : domain(other.domain),
      component_id(other.component_id) {}

PolicyNamespace::~PolicyNamespace() {}

PolicyNamespace& PolicyNamespace::operator=(const PolicyNamespace& other) {
  domain = other.domain;
  component_id = other.component_id;
  return *this;
}

bool PolicyNamespace::operator<(const PolicyNamespace& other) const {
  return domain < other.domain ||
         (domain == other.domain && component_id < other.component_id);
}

bool PolicyNamespace::operator==(const PolicyNamespace& other) const {
  return domain == other.domain && component_id == other.component_id;
}

bool PolicyNamespace::operator!=(const PolicyNamespace& other) const {
  return !(*this == other);
}

}  // namespace policy
