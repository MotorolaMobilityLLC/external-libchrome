// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_
#define MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "mojo/public/bindings/lib/bindings_internal.h"

namespace mojo {

template <typename T, typename U> class SimilarityTraits {};

// Provides read-only access to array data.
template <typename T>
class Array {
 public:
  typedef internal::ArrayTraits<T, internal::TypeTraits<T>::kIsObject> Traits_;
  typedef typename Traits_::DataType Data;

  Array() : data_(NULL) {
  }

  template <typename U>
  Array(const U& u, Buffer* buf = Buffer::current()) {
    *this = SimilarityTraits<Array<T>,U>::CopyFrom(u, buf);
  }

  template <typename U>
  Array& operator=(const U& u) {
    *this = SimilarityTraits<Array<T>,U>::CopyFrom(u, Buffer::current());
    return *this;
  }

  template <typename U>
  U To() const {
    return SimilarityTraits<Array<T>,U>::CopyTo(*this);
  }

  bool is_null() const { return !data_; }

  size_t size() const { return data_->size(); }

  const T& at(size_t offset) const {
    return Traits_::ToConstRef(data_->at(offset));
  }
  const T& operator[](size_t offset) const { return at(offset); }

  // Provides a way to initialize an array element-by-element.
  class Builder {
   public:
    typedef typename Array<T>::Data Data;
    typedef typename Array<T>::Traits_ Traits_;

    explicit Builder(size_t num_elements, Buffer* buf = mojo::Buffer::current())
        : data_(Data::New(num_elements, buf)) {
    }

    size_t size() const { return data_->size(); }

    T& at(size_t offset) {
      return Traits_::ToRef(data_->at(offset));
    }
    T& operator[](size_t offset) { return at(offset); }

    Array<T> Finish() {
      Data* data = NULL;
      std::swap(data, data_);
      return internal::Wrap(data);
    }

   private:
    Data* data_;
    MOJO_DISALLOW_COPY_AND_ASSIGN(Builder);
  };

 protected:
  friend class internal::WrapperHelper<Array<T> >;

  struct Wrap {};
  Array(Wrap, const Data* data) : data_(data) {}

  const Data* data_;
};

// UTF-8 encoded
typedef Array<char> String;

template <>
class SimilarityTraits<String, std::string> {
 public:
  static String CopyFrom(const std::string& input, Buffer* buf) {
    String::Builder result(input.size(), buf);
    memcpy(&result[0], input.data(), input.size());
    return result.Finish();
  }
  static std::string CopyTo(const String& input) {
    return std::string(&input[0], &input[0] + input.size());
  }
};

template <size_t N>
class SimilarityTraits<String, char[N]> {
 public:
  static String CopyFrom(const char input[N], Buffer* buf) {
    String::Builder result(N - 1, buf);
    memcpy(&result[0], input, N - 1);
    return result.Finish();
  }
};

// Appease MSVC.
template <size_t N>
class SimilarityTraits<String, const char[N]> {
 public:
  static String CopyFrom(const char input[N], Buffer* buf) {
    return SimilarityTraits<String, char[N]>::CopyFrom(input, buf);
  }
};

template <>
class SimilarityTraits<String, const char*> {
 public:
  static String CopyFrom(const char* input, Buffer* buf) {
    size_t size = strlen(input);
    String::Builder result(size, buf);
    memcpy(&result[0], input, size);
    return result.Finish();
  }
  // NOTE: |CopyTo| explicitly not implemented since String is not null
  // terminated (and may have embedded null bytes).
};

template <typename T, typename E>
class SimilarityTraits<Array<T>, std::vector<E> > {
 public:
  static Array<T> CopyFrom(const std::vector<E>& input, Buffer* buf) {
    typename Array<T>::Builder result(input.size(), buf);
    for (size_t i = 0; i < input.size(); ++i)
      result[i] = SimilarityTraits<T, E>::CopyFrom(input[i], buf);
    return result.Finish();
  }
  static std::vector<E> CopyTo(const Array<T>& input) {
    std::vector<E> result(input.size());
    for (size_t i = 0; i < input.size(); ++i)
      result[i] = SimilarityTraits<T, E>::CopyTo(input[i]);
    return result;
  }
};


}  // namespace mojo

#endif  // MOJO_PUBLIC_BINDINGS_LIB_BINDINGS_H_
