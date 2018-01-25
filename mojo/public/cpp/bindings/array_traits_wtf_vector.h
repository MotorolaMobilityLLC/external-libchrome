// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_VECTOR_H_
#define MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_VECTOR_H_

#include "mojo/public/cpp/bindings/array_traits.h"
#include "third_party/WebKit/Source/platform/wtf/Vector.h"

namespace mojo {

template <typename U, size_t InlineCapacity>
struct ArrayTraits<WTF::Vector<U, InlineCapacity>> {
  using Element = U;

  static bool IsNull(const WTF::Vector<U, InlineCapacity>& input) {
    // WTF::Vector<> is always converted to non-null mojom array.
    return false;
  }

  static void SetToNull(WTF::Vector<U, InlineCapacity>* output) {
    // WTF::Vector<> doesn't support null state. Set it to empty instead.
    output->clear();
  }

  static size_t GetSize(const WTF::Vector<U, InlineCapacity>& input) {
    return input.size();
  }

  static U* GetData(WTF::Vector<U, InlineCapacity>& input) {
    return input.data();
  }

  static const U* GetData(const WTF::Vector<U, InlineCapacity>& input) {
    return input.data();
  }

  static U& GetAt(WTF::Vector<U, InlineCapacity>& input, size_t index) {
    return input[index];
  }

  static const U& GetAt(const WTF::Vector<U, InlineCapacity>& input,
                        size_t index) {
    return input[index];
  }

  static bool Resize(WTF::Vector<U, InlineCapacity>& input, size_t size) {
    input.resize(size);
    return true;
  }
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_ARRAY_TRAITS_WTF_VECTOR_H_
