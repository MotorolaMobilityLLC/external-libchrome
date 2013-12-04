// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/bindings.h"

namespace mojo {

// static
String SimilarityTraits<String, std::string>::CopyFrom(const std::string& input,
                                                       Buffer* buf) {
  String::Builder result(input.size(), buf);
  if (!input.empty())
    memcpy(&result[0], input.data(), input.size());
  return result.Finish();
}
// static
std::string SimilarityTraits<String, std::string>::CopyTo(const String& input) {
  if (input.is_null() || input.size() == 0)
    return std::string();

  return std::string(&input[0], &input[0] + input.size());
}

// static
String SimilarityTraits<String, const char*>::CopyFrom(const char* input,
                                                       Buffer* buf) {
  if (!input)
    return String();

  size_t size = strlen(input);
  String::Builder result(size, buf);
  if (size != 0)
    memcpy(&result[0], input, size);
  return result.Finish();
}

}  // namespace mojo
