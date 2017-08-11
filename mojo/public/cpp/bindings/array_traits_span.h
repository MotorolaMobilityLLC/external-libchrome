// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_

#include <cstddef>

#include "base/containers/span.h"
#include "mojo/public/cpp/bindings/array_traits.h"

namespace mojo {

// TODO(dcheng): Remove these type aliases.
template <typename T>
using CArray = base::Span<T>;

template <typename T>
using ConstCArray = base::Span<const T>;

template <typename T>
struct ArrayTraits<base::Span<T>> {
  using Element = T;

  static bool IsNull(const base::Span<T>& input) { return !input.data(); }

  static void SetToNull(base::Span<T>* output) { *output = base::Span<T>(); }

  static size_t GetSize(const base::Span<T>& input) { return input.size(); }

  static T* GetData(base::Span<T>& input) { return input.data(); }

  static const T* GetData(const base::Span<T>& input) { return input.data(); }

  static T& GetAt(base::Span<T>& input, size_t index) {
    return input.data()[index];
  }

  static const T& GetAt(const base::Span<T>& input, size_t index) {
    return input.data()[index];
  }

  static bool Resize(base::Span<T>& input, size_t size) {
    if (size > input.size())
      return false;
    input = input.subspan(0, size);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_CARRAY_H_
