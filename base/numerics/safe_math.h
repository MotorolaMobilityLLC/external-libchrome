// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_SAFE_MATH_H_
#define BASE_NUMERICS_SAFE_MATH_H_

#include <stddef.h>

#include <limits>
#include <type_traits>

#include "base/numerics/safe_math_impl.h"

namespace base {
namespace internal {

// CheckedNumeric implements all the logic and operators for detecting integer
// boundary conditions such as overflow, underflow, and invalid conversions.
// The CheckedNumeric type implicitly converts from floating point and integer
// data types, and contains overloads for basic arithmetic operations (i.e.: +,
// -, *, / for all types, % for integers, and &, |, ^ for unsigned integers).
//
// You may also use one of the variadic convenience functions, which accept
// standard arithmetic or CheckedNumeric types, perform arithmetic operations,
// and return a CheckedNumeric result. The supported functions are:
//  CheckAdd() - Addition.
//  CheckSub() - Subtraction.
//  CheckMul() - Multiplication.
//  CheckDiv() - Division.
//  CheckMod() - Modulous (integer only).
//  CheckLsh() - Left integer shift (integer only).
//  CheckRsh() - Right integer shift (integer only).
//
// The unary negation, increment, and decrement operators are supported, along
// with the following unary arithmetic methods, which return a new
// CheckedNumeric as a result of the operation:
//  Abs() - Absolute value.
//  UnsignedAbs() - Absolute value as an equival-width unsigned underlying type
//          (valid for only integral types).
//
// The following methods convert from CheckedNumeric to standard numeric values:
//  IsValid() - Returns true if the underlying numeric value is valid (i.e. has
//          has not wrapped and is not the result of an invalid conversion).
//  ValueOrDie() - Returns the underlying value. If the state is not valid this
//          call will crash on a CHECK.
//  ValueOrDefault() - Returns the current value, or the supplied default if the
//          state is not valid (will not trigger a CHECK).
//  ValueFloating() - Returns the underlying floating point value (valid only
//          for floating point CheckedNumeric types; will not cause a CHECK).
//
// The following are general utility methods that are useful for converting
// between arithmetic types and CheckedNumeric types:
//  CheckedNumeric::Cast<Dst>() - Instance method returning a CheckedNumeric
//          derived from casting the current instance to a CheckedNumeric of
//          the supplied destination type.
//  CheckNum() - Creates a new CheckedNumeric from the underlying type of the
//          supplied arithmetic or CheckedNumeric type.
//
// Comparison operations are explicitly not supported because they could result
// in a crash on an unexpected CHECK condition. You should use patterns like the
// following for comparisons:
//   CheckedNumeric<size_t> checked_size = untrusted_input_value;
//   checked_size += HEADER LENGTH;
//   if (checked_size.IsValid() && checked_size.ValueOrDie() < buffer_size)
//     Do stuff...

template <typename T>
class CheckedNumeric {
  static_assert(std::is_arithmetic<T>::value,
                "CheckedNumeric<T>: T must be a numeric type.");

 public:
  typedef T type;

  constexpr CheckedNumeric() {}

  // Copy constructor.
  template <typename Src>
  constexpr CheckedNumeric(const CheckedNumeric<Src>& rhs)
      : state_(rhs.state_.value(), rhs.IsValid()) {}

  template <typename Src>
  friend class CheckedNumeric;

  // This is not an explicit constructor because we implicitly upgrade regular
  // numerics to CheckedNumerics to make them easier to use.
  template <typename Src>
  constexpr CheckedNumeric(Src value)  // NOLINT(runtime/explicit)
      : state_(value) {
    static_assert(std::numeric_limits<Src>::is_specialized,
                  "Argument must be numeric.");
  }

  // This is not an explicit constructor because we want a seamless conversion
  // from StrictNumeric types.
  template <typename Src>
  constexpr CheckedNumeric(
      StrictNumeric<Src> value)  // NOLINT(runtime/explicit)
      : state_(static_cast<Src>(value)) {}

  // IsValid() is the public API to test if a CheckedNumeric is currently valid.
  constexpr bool IsValid() const { return state_.is_valid(); }

  // ValueOrDie() The primary accessor for the underlying value. If the current
  // state is not valid it will CHECK and crash.
  constexpr T ValueOrDie() const {
    return IsValid() ? state_.value() : CheckOnFailure::HandleFailure<T>();
  }

  // ValueOrDefault(T default_value) A convenience method that returns the
  // current value if the state is valid, and the supplied default_value for
  // any other state.
  constexpr T ValueOrDefault(T default_value) const {
    return IsValid() ? state_.value() : default_value;
  }

  // ValueFloating() - Since floating point values include their validity state,
  // we provide an easy method for extracting them directly, without a risk of
  // crashing on a CHECK.
  constexpr T ValueFloating() const {
    static_assert(std::numeric_limits<T>::is_iec559, "Argument must be float.");
    return state_.value();
  }

  // Returns a checked numeric of the specified type, cast from the current
  // CheckedNumeric. If the current state is invalid or the destination cannot
  // represent the result then the returned CheckedNumeric will be invalid.
  template <typename Dst>
  constexpr CheckedNumeric<typename UnderlyingType<Dst>::type> Cast() const {
    return *this;
  }

  // This friend method is available solely for providing more detailed logging
  // in the the tests. Do not implement it in production code, because the
  // underlying values may change at any time.
  template <typename U>
  friend U GetNumericValueForTest(const CheckedNumeric<U>& src);

  // Prototypes for the supported arithmetic operator overloads.
  template <typename Src>
  CheckedNumeric& operator+=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator-=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator*=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator/=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator%=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator<<=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator>>=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator&=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator|=(const Src rhs);
  template <typename Src>
  CheckedNumeric& operator^=(const Src rhs);

  CheckedNumeric operator-() const {
    // Negation is always valid for floating point.
    T value = 0;
    bool is_valid = (std::numeric_limits<T>::is_iec559 || IsValid()) &&
                    CheckedNeg(state_.value(), &value);
    return CheckedNumeric<T>(value, is_valid);
  }

  CheckedNumeric operator~() const {
    static_assert(!std::is_signed<T>::value, "Type must be unsigned.");
    T value = 0;
    bool is_valid = IsValid() && CheckedInv(state_.value(), &value);
    return CheckedNumeric<T>(value, is_valid);
  }

  CheckedNumeric Abs() const {
    // Absolute value is always valid for floating point.
    T value = 0;
    bool is_valid = (std::numeric_limits<T>::is_iec559 || IsValid()) &&
                    CheckedAbs(state_.value(), &value);
    return CheckedNumeric<T>(value, is_valid);
  }

  // This function is available only for integral types. It returns an unsigned
  // integer of the same width as the source type, containing the absolute value
  // of the source, and properly handling signed min.
  constexpr CheckedNumeric<typename UnsignedOrFloatForSize<T>::type>
  UnsignedAbs() const {
    return CheckedNumeric<typename UnsignedOrFloatForSize<T>::type>(
        SafeUnsignedAbs(state_.value()), state_.is_valid());
  }

  CheckedNumeric& operator++() {
    *this += 1;
    return *this;
  }

  CheckedNumeric operator++(int) {
    CheckedNumeric value = *this;
    *this += 1;
    return value;
  }

  CheckedNumeric& operator--() {
    *this -= 1;
    return *this;
  }

  CheckedNumeric operator--(int) {
    CheckedNumeric value = *this;
    *this -= 1;
    return value;
  }

  // These perform the actual math operations on the CheckedNumerics.
  // Binary arithmetic operations.
  template <template <typename, typename, typename> class M,
            typename L,
            typename R>
  static CheckedNumeric MathOp(const L& lhs, const R& rhs) {
    using Math = typename MathWrapper<M, L, R>::math;
    T result = 0;
    bool is_valid =
        Wrapper<L>::is_valid(lhs) && Wrapper<R>::is_valid(rhs) &&
        Math::Do(Wrapper<L>::value(lhs), Wrapper<R>::value(rhs), &result);
    return CheckedNumeric<T>(result, is_valid);
  };

  // Assignment arithmetic operations.
  template <template <typename, typename, typename> class M, typename R>
  CheckedNumeric& MathOp(const R& rhs) {
    using Math = typename MathWrapper<M, T, R>::math;
    T result = 0;  // Using T as the destination saves a range check.
    bool is_valid = state_.is_valid() && Wrapper<R>::is_valid(rhs) &&
                    Math::Do(state_.value(), Wrapper<R>::value(rhs), &result);
    *this = CheckedNumeric<T>(result, is_valid);
    return *this;
  };

 private:
  CheckedNumericState<T> state_;

  template <typename Src>
  constexpr CheckedNumeric(Src value, bool is_valid)
      : state_(value, is_valid) {}

  // These wrappers allow us to handle state the same way for both
  // CheckedNumeric and POD arithmetic types.
  template <typename Src>
  struct Wrapper {
    static constexpr bool is_valid(Src) { return true; }
    static constexpr Src value(Src value) { return value; }
  };

  template <typename Src>
  struct Wrapper<CheckedNumeric<Src>> {
    static constexpr bool is_valid(const CheckedNumeric<Src>& v) {
      return v.IsValid();
    }
    static constexpr Src value(const CheckedNumeric<Src>& v) {
      return v.state_.value();
    }
  };
};

// These variadic templates work out the return types.
// TODO(jschuh): Rip all this out once we have C++14 non-trailing auto support.
template <template <typename, typename, typename> class M,
          typename L,
          typename R,
          typename... Args>
struct ResultType;

template <template <typename, typename, typename> class M,
          typename L,
          typename R>
struct ResultType<M, L, R> {
  using type = typename MathWrapper<M, L, R>::type;
};

template <template <typename, typename, typename> class M,
          typename L,
          typename R,
          typename... Args>
struct ResultType {
  // The typedef was required here because MSVC fails to compile with "using".
  typedef
      typename ResultType<M, typename ResultType<M, L, R>::type, Args...>::type
          type;
};

// Convience wrapper to return a new CheckedNumeric from the provided arithmetic
// or CheckedNumericType.
template <typename T>
CheckedNumeric<typename UnderlyingType<T>::type> CheckNum(T value) {
  return value;
}

// These implement the variadic wrapper for the math operations.
template <template <typename, typename, typename> class M,
          typename L,
          typename R>
CheckedNumeric<typename MathWrapper<M, L, R>::type> ChkMathOp(const L lhs,
                                                              const R rhs) {
  using Math = typename MathWrapper<M, L, R>::math;
  return CheckedNumeric<typename Math::result_type>::template MathOp<M>(lhs,
                                                                        rhs);
};

template <template <typename, typename, typename> class M,
          typename L,
          typename R,
          typename... Args>
CheckedNumeric<typename ResultType<M, L, R, Args...>::type>
ChkMathOp(const L lhs, const R rhs, const Args... args) {
  auto tmp = ChkMathOp<M>(lhs, rhs);
  return tmp.IsValid() ? ChkMathOp<M>(tmp, args...)
                       : decltype(ChkMathOp<M>(tmp, args...))(tmp);
};

// This is just boilerplate for the standard arithmetic operator overloads.
// A macro isn't the nicest solution, but it beats rewriting these repeatedly.
#define BASE_NUMERIC_ARITHMETIC_OPERATORS(NAME, OP, COMPOUND_OP)               \
  /* Binary arithmetic operator for all CheckedNumeric operations. */          \
  template <typename L, typename R,                                            \
            typename std::enable_if<IsCheckedOp<L, R>::value>::type* =         \
                nullptr>                                                       \
  CheckedNumeric<typename MathWrapper<Checked##NAME##Op, L, R>::type>          \
  operator OP(const L lhs, const R rhs) {                                      \
    return decltype(lhs OP rhs)::template MathOp<Checked##NAME##Op>(lhs, rhs); \
  }                                                                            \
  /* Assignment arithmetic operator implementation from CheckedNumeric. */     \
  template <typename L>                                                        \
  template <typename R>                                                        \
  CheckedNumeric<L>& CheckedNumeric<L>::operator COMPOUND_OP(const R rhs) {    \
    return MathOp<Checked##NAME##Op>(rhs);                                     \
  }                                                                            \
  /* Variadic arithmetic functions that return CheckedNumeric. */              \
  template <typename L, typename R, typename... Args>                          \
  CheckedNumeric<typename ResultType<Checked##NAME##Op, L, R, Args...>::type>  \
      Check##NAME(const L lhs, const R rhs, const Args... args) {              \
    return ChkMathOp<Checked##NAME##Op, L, R, Args...>(lhs, rhs, args...);     \
  }

BASE_NUMERIC_ARITHMETIC_OPERATORS(Add, +, +=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Sub, -, -=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Mul, *, *=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Div, /, /=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Mod, %, %=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Lsh, <<, <<=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Rsh, >>, >>=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(And, &, &=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Or, |, |=)
BASE_NUMERIC_ARITHMETIC_OPERATORS(Xor, ^, ^=)

#undef BASE_NUMERIC_ARITHMETIC_OPERATORS

}  // namespace internal

using internal::CheckedNumeric;
using internal::CheckNum;
using internal::CheckAdd;
using internal::CheckSub;
using internal::CheckMul;
using internal::CheckDiv;
using internal::CheckMod;
using internal::CheckLsh;
using internal::CheckRsh;
using internal::CheckAnd;
using internal::CheckOr;
using internal::CheckXor;

}  // namespace base

#endif  // BASE_NUMERICS_SAFE_MATH_H_
