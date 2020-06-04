// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/rand_util.h"

#include <math.h>

#include <limits>

#include "base/basictypes.h"
#include "base/logging.h"

namespace base {

int RandInt(int min, int max) {
  DCHECK_LE(min, max);

  uint64 range = static_cast<uint64>(max) - min + 1;
  int result = min + static_cast<int>(base::RandGenerator(range));
  DCHECK_GE(result, min);
  DCHECK_LE(result, max);
  return result;
}

double RandDouble() {
  return BitsToOpenEndedUnitInterval(base::RandUint64());
}

double BitsToOpenEndedUnitInterval(uint64 bits) {
  // We try to get maximum precision by masking out as many bits as will fit
  // in the target type's mantissa, and raising it to an appropriate power to
  // produce output in the range [0, 1).  For IEEE 754 doubles, the mantissa
  // is expected to accommodate 53 bits.

  COMPILE_ASSERT(std::numeric_limits<double>::radix == 2, otherwise_use_scalbn);
  static const int kBits = std::numeric_limits<double>::digits;
  uint64 random_bits = bits & ((GG_UINT64_C(1) << kBits) - 1);
  double result = ldexp(static_cast<double>(random_bits), -1 * kBits);
  DCHECK_GE(result, 0.0);
  DCHECK_LT(result, 1.0);
  return result;
}

uint64 RandGenerator(uint64 max) {
  DCHECK_GT(max, 0ULL);
  return base::RandUint64() % max;
}

std::string RandBytesAsString(size_t length) {
  const size_t kBitsPerChar = 8;
  const int kCharsPerInt64 = sizeof(uint64)/sizeof(char);

  std::string result(length, '\0');
  uint64 entropy = 0;
  for (size_t i = 0; i < result.size(); ++i) {
    if (i % kCharsPerInt64 == 0)
      entropy = RandUint64();
    result[i] = static_cast<char>(entropy);
    entropy >>= kBitsPerChar;
  }

  return result;
}

}  // namespace base
