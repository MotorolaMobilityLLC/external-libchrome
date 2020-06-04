// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_SAFE_NUMERICS_H_
#define BASE_SAFE_NUMERICS_H_

#include <limits>

#include "base/logging.h"
#include "base/safe_numerics_impl.h"

namespace base {

// Convenience function that returns true if the supplied value is in range
// for the destination type.
template <typename Dst, typename Src>
inline bool IsValueInRangeForNumericType(Src value) {
  return internal::RangeCheck<Dst>(value) == internal::TYPE_VALID;
}

// checked_numeric_cast<> is analogous to static_cast<> for numeric types,
// except that it CHECKs that the specified numeric conversion will not
// overflow or underflow. NaN source will always trigger a CHECK.
template <typename Dst, typename Src>
inline Dst checked_numeric_cast(Src value) {
  CHECK(IsValueInRangeForNumericType<Dst>(value));
  return static_cast<Dst>(value);
}

// saturated_cast<> is analogous to static_cast<> for numeric types, except
// that the specified numeric conversion will saturate rather than overflow or
// underflow. NaN assignment to an integral will trigger a CHECK condition.
template <typename Dst, typename Src>
inline Dst saturated_cast(Src value) {
  // Optimization for floating point values, which already saturate.
  if (std::numeric_limits<Dst>::is_iec559)
    return static_cast<Dst>(value);

  switch (internal::RangeCheck<Dst>(value)) {
    case internal::TYPE_VALID:
      return static_cast<Dst>(value);

    case internal::TYPE_UNDERFLOW:
      return std::numeric_limits<Dst>::min();

    case internal::TYPE_OVERFLOW:
      return std::numeric_limits<Dst>::max();

    // Should fail only on attempting to assign NaN to a saturated integer.
    case internal::TYPE_INVALID:
      CHECK(false);
      return std::numeric_limits<Dst>::max();
  }

  NOTREACHED();
  return static_cast<Dst>(value);
}

}  // namespace base

#endif  // BASE_SAFE_NUMERICS_H_
