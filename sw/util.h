// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _UTIL_H
#define _UTIL_H

extern "C" {

#include <stdint.h>

}  // extern "C"

template <typename T>
T&& move(T& ref) {
  return static_cast<T&&>(ref);
}

template <typename T, typename U>
T exchange(T& ref, U new_value) {
  T result = move<T>(ref);
  ref = move<U>(new_value);
  return result;
}

// A fixed-width fraction. The default types allow to represent values within
// [0..1].
template <typename T = uint_fast16_t, uint8_t Bits = sizeof(T) * 8 - 2>
struct FixedPointFraction {
  using value_type = T;
  constexpr static uint8_t kFractionBits = Bits;

  constexpr explicit FixedPointFraction(T fraction_bits_)
      : fraction_bits(fraction_bits_) {}
  constexpr FixedPointFraction(float f)
      : FixedPointFraction(static_cast<T>(f * (1 << Bits))) {}

  T fraction_bits;
};

#endif  // _UTIL_H
