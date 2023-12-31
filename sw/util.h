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

template <typename T>
struct identity {
  typedef T type;
};
// See https://stackoverflow.com/q/27501400/1333025.
template <typename T>
T&& forward(typename identity<T>::type& param) {
  return static_cast<typename identity<T>::type&&>(param);
}

template <typename T>
class optional {
 public:
  using value_type = T;

  optional() : no_value_placeholder_(), has_value_(false) {}
  optional(T value) { emplace(move(value)); }
  ~optional() { reset(); }

  bool has_value() const { return has_value_; }
  operator bool() const { return has_value(); }

  const T& operator*() const& { return value_; }
  T& operator*() & { return value_; }
  const T&& operator*() const&& { return move(value_); }
  T&& operator*() && { return move(value_); }

  T* operator->() { return &**this; }
  const T* operator->() const { return &**this; }

  template <typename... Args>
  T& emplace(Args... args) {
    if (exchange(has_value_, true)) {
      value_.~T();
    }
    value_ = T(args...);
    return value_;
  }

  void reset() {
    if (exchange(has_value_, false)) {
      value_.~T();
    }
    no_value_placeholder_ = {};
  }

 private:
  union {
    struct {
    } no_value_placeholder_;
    T value_;
  };
  bool has_value_;
};

// A fixed-width fraction. The default types allow to represent values within
// [-1..1].
template <typename T = int_fast16_t, uint8_t Bits = sizeof(T) * 8 - 2>
struct FixedPointFraction {
  using value_type = T;
  constexpr static uint8_t kFractionBits = Bits;

  constexpr explicit FixedPointFraction(T fraction_bits_)
      : fraction_bits(fraction_bits_) {}
  constexpr FixedPointFraction(float f)
      : FixedPointFraction(static_cast<T>(f * (1 << Bits))) {}

  // Auto-converts to any `FixedPointFraction` type of higher precision,
  // shifting right(+) or left(-) at the same time.
  template <int8_t RightShift>
  struct AutoConverter {
    template <typename U, uint8_t UBits = sizeof(U) * 8 - 2>
    operator FixedPointFraction<U, UBits>() const&& {
      using Target = FixedPointFraction<U, UBits>;
      static constexpr int kLeftShift =
          Target::kFractionBits - kFractionBits - RightShift;
      static_assert(
          kLeftShift >= 0,
          "The conversion would lose precision (if needed, this check could be "
          "losened to allow right-shifting up to the RightShift parameter)");
      return Target(static_cast<U>(fraction.fraction_bits << kLeftShift));
    }

    const FixedPointFraction& fraction;
  };

  // Converts from this representation to another of the same or higher
  // precision.
  AutoConverter<0> Convert() const { return AutoConverter<0>{*this}; }
  // Shifts this value right (positive) or left (negative `shift`)
  template <int8_t Shift>
  AutoConverter<Shift> ShiftRight() const {
    return AutoConverter<Shift>{*this};
  }

  T fraction_bits;
};

#endif  // _UTIL_H
