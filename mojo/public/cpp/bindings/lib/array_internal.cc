// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/array_internal.h"

namespace mojo {
namespace internal {

ArrayDataTraits<bool>::BitRef::~BitRef() {
}

ArrayDataTraits<bool>::BitRef::BitRef(uint8_t* storage, uint8_t mask)
    : storage_(storage),
      mask_(mask) {
}

ArrayDataTraits<bool>::BitRef&
ArrayDataTraits<bool>::BitRef::operator=(bool value) {
  if (value) {
    *storage_ |= mask_;
  } else {
    *storage_ &= ~mask_;
  }
  return *this;
}

ArrayDataTraits<bool>::BitRef&
ArrayDataTraits<bool>::BitRef::operator=(const BitRef& value) {
  return (*this) = static_cast<bool>(value);
}

ArrayDataTraits<bool>::BitRef::operator bool() const {
  return (*storage_ & mask_) != 0;
}

// static
void ArraySerializationHelper<Handle, true>::EncodePointersAndHandles(
    const ArrayHeader* header,
    ElementType* elements,
    std::vector<Handle>* handles) {
  for (uint32_t i = 0; i < header->num_elements; ++i)
    EncodeHandle(&elements[i], handles);
}

// static
bool ArraySerializationHelper<Handle, true>::DecodePointersAndHandles(
    const ArrayHeader* header,
    ElementType* elements,
    Message* message) {
  for (uint32_t i = 0; i < header->num_elements; ++i) {
    if (!DecodeHandle(&elements[i], message->mutable_handles()))
      return false;
  }
  return true;
}

}  // namespace internal
}  // namespace mojo
