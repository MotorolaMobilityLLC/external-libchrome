// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SPAN_H_
#define BASE_SPAN_H_

#include <stddef.h>

#include <algorithm>
#include <array>
#include <type_traits>
#include <utility>

namespace base {

template <typename T>
class span;

namespace internal {

template <typename T>
struct IsSpanImpl : std::false_type {};

template <typename T>
struct IsSpanImpl<span<T>> : std::true_type {};

template <typename T>
using IsSpan = IsSpanImpl<std::decay_t<T>>;

template <typename T>
struct IsStdArrayImpl : std::false_type {};

template <typename T, size_t N>
struct IsStdArrayImpl<std::array<T, N>> : std::true_type {};

template <typename T>
using IsStdArray = IsStdArrayImpl<std::decay_t<T>>;

template <typename From, typename To>
using IsLegalSpanConversion = std::is_convertible<From*, To*>;

template <typename Container, typename T>
using ContainerHasConvertibleData = IsLegalSpanConversion<
    std::remove_pointer_t<decltype(std::declval<Container>().data())>,
    T>;
template <typename Container>
using ContainerHasIntegralSize =
    std::is_integral<decltype(std::declval<Container>().size())>;

template <typename From, typename To>
using EnableIfLegalSpanConversion =
    std::enable_if_t<IsLegalSpanConversion<From, To>::value>;

// SFINAE check if Container can be converted to a span<T>. Note that the
// implementation details of this check differ slightly from the requirements in
// the working group proposal: in particular, the proposal also requires that
// the container conversion constructor participate in overload resolution only
// if two additional conditions are true:
//
//   1. Container implements operator[].
//   2. Container::value_type matches remove_const_t<element_type>.
//
// The requirements are relaxed slightly here: in particular, not requiring (2)
// means that an immutable span can be easily constructed from a mutable
// container.
template <typename Container, typename T>
using EnableIfSpanCompatibleContainer =
    std::enable_if_t<!internal::IsSpan<Container>::value &&
                     !internal::IsStdArray<Container>::value &&
                     ContainerHasConvertibleData<Container, T>::value &&
                     ContainerHasIntegralSize<Container>::value>;

template <typename Container, typename T>
using EnableIfConstSpanCompatibleContainer =
    std::enable_if_t<std::is_const<T>::value &&
                     !internal::IsSpan<Container>::value &&
                     !internal::IsStdArray<Container>::value &&
                     ContainerHasConvertibleData<Container, T>::value &&
                     ContainerHasIntegralSize<Container>::value>;

}  // namespace internal

// A span is a value type that represents an array of elements of type T.  It
// consists of a pointer to memory with an associated size. A span does not own
// the underlying memory, so care must be taken to ensure that a span does not
// outlive the backing store.
//
// span is somewhat analogous to StringPiece, but with arbitrary element types,
// allowing mutation if T is non-const.
//
// span is implicitly convertible from C++ arrays, as well as most [1]
// container-like types that provide a data() and size() method (such as
// std::vector<T>). A mutable span<T> can also be implicitly converted to an
// immutable span<const T>.
//
// Consider using a span for functions that take a data pointer and size
// parameter: it allows the function to still act on an array-like type, while
// allowing the caller code to be a bit more concise.
//
// Without span:
//   // std::string HexEncode(uint8_t* data, size_t size);
//   std::vector<uint8_t> data_buffer = GenerateData();
//   std::string r = HexEncode(data_buffer.data(), data_buffer.size());
//
// With span:
//   // std::string HexEncode(base::span<uint8_t> data);
//   std::vector<uint8_t> data_buffer = GenerateData();
//   std::string r = HexEncode(data_buffer);
//
// ======= Differences from the working group proposal =======
// http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0122r5.pdf is the
// latest working group proposal. The biggest difference is span does not
// support a static extent template parameter. Other differences are documented
// in subsections below.
//
// Differences in constants and types:
// - no element_type type alias
// - no index_type type alias
// - no different_type type alias
//
// Differences from [span.cons]:
// - no constructor from a pointer range
// - no constructor from std::array
// - no constructor from std::unique_ptr
// - no constructor from std::shared_ptr
// - no explicitly defaulted the copy/move constructor/assignment operators,
//   since MSVC complains about constexpr functions that aren't marked const.
//
// Differences from [span.sub]:
// - no first()
// - no last()
// - no templated subspan()
//
// Differences from [span.elem]:
// - no operator ()()
//
// Differences from [span.iter]:
// - no reverse iterators
//
// Differences from [span.comparison]:
// - no operator <()
// - no operator <=()
// - no operator >()
// - no operator >=()
//
// Differences from [span.objectrep]:
// - no as_bytes()
// - no as_writeable_bytes()
template <typename T>
class span {
 public:
  using value_type = std::remove_cv_t<T>;
  using pointer = T*;
  using reference = T&;
  using iterator = T*;
  using const_iterator = const T*;
  // TODO(dcheng): What about reverse iterators?

  // Span constructors, copy, assignment, and destructor
  constexpr span() noexcept : data_(nullptr), size_(0) {}
  constexpr span(T* data, size_t size) noexcept : data_(data), size_(size) {}
  // TODO(dcheng): Implement construction from a |begin| and |end| pointer.
  template <size_t N>
  constexpr span(T (&array)[N]) noexcept : span(array, N) {}
  // TODO(dcheng): Implement construction from std::array.
  // Conversion from a container that provides |T* data()| and |integral_type
  // size()|.
  template <typename Container,
            typename = internal::EnableIfSpanCompatibleContainer<Container, T>>
  constexpr span(Container& container)
      : span(container.data(), container.size()) {}
  template <
      typename Container,
      typename = internal::EnableIfConstSpanCompatibleContainer<Container, T>>
  span(const Container& container) : span(container.data(), container.size()) {}
  ~span() noexcept = default;
  // Conversions from spans of compatible types: this allows a span<T> to be
  // seamlessly used as a span<const T>, but not the other way around.
  template <typename U, typename = internal::EnableIfLegalSpanConversion<U, T>>
  constexpr span(const span<U>& other) : span(other.data(), other.size()) {}
  template <typename U, typename = internal::EnableIfLegalSpanConversion<U, T>>
  constexpr span(span<U>&& other) : span(other.data(), other.size()) {}

  // Span subviews
  constexpr span subspan(size_t pos, size_t count) const {
    // Note: ideally this would DCHECK, but it requires fairly horrible
    // contortions.
    return span(data_ + pos, count);
  }

  // Span observers
  constexpr size_t size() const noexcept { return size_; }

  // Span element access
  constexpr T& operator[](size_t index) const noexcept { return data_[index]; }
  constexpr T* data() const noexcept { return data_; }

  // Span iterator support
  iterator begin() const noexcept { return data_; }
  iterator end() const noexcept { return data_ + size_; }

  const_iterator cbegin() const noexcept { return begin(); }
  const_iterator cend() const noexcept { return end(); }

 private:
  T* data_;
  size_t size_;
};

// Relational operators. Equality is a element-wise comparison.
template <typename T>
constexpr bool operator==(const span<T>& lhs, const span<T>& rhs) noexcept {
  return std::equal(lhs.cbegin(), lhs.cend(), rhs.cbegin(), rhs.cend());
}

template <typename T>
constexpr bool operator!=(const span<T>& lhs, const span<T>& rhs) noexcept {
  return !(lhs == rhs);
}

// TODO(dcheng): Implement other relational operators.

// Type-deducing helpers for constructing a span.
template <typename T>
constexpr span<T> make_span(T* data, size_t size) noexcept {
  return span<T>(data, size);
}

template <typename T, size_t N>
constexpr span<T> make_span(T (&array)[N]) noexcept {
  return span<T>(array);
}

template <typename Container,
          typename T = typename Container::value_type,
          typename = internal::EnableIfSpanCompatibleContainer<Container, T>>
constexpr span<T> make_span(Container& container) {
  return span<T>(container);
}

template <
    typename Container,
    typename T = std::add_const_t<typename Container::value_type>,
    typename = internal::EnableIfConstSpanCompatibleContainer<Container, T>>
constexpr span<T> make_span(const Container& container) {
  return span<T>(container);
}

}  // namespace base

#endif  // BASE_SPAN_H_
