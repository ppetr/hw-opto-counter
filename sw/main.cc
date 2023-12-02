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

extern "C" {

#include <avr/io.h>
#include <avr/signature.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdlib.h>
#include <util/atomic.h>

}  // extern "C"

#include "timer.h"
#include "twi.h"

constexpr uint8_t kTwiAddress =
    18;  // Randomly generated - https://xkcd.com/221/

class Sleep {
 public:
  explicit Sleep(SLPCTRL_SMODE_enum mode) {
    set_sleep_mode(mode & SLPCTRL_SMODE_gm);
    sleep_enable();
  }
  ~Sleep() { sleep_disable(); }

  void Start() {
    NONATOMIC_BLOCK(NONATOMIC_RESTORESTATE) { sleep_mode(); };
  }
};

struct InputPin {
  InputPin(PORT_t& port_, register8_t bitmask_)
      : port(port_.IN), bitmask(bitmask_) {
    port_.DIRCLR = bitmask;
  }

  bool Read() const { return port & bitmask; }

  const register8_t& port;
  register8_t bitmask;
};

class BinarySearch {
 public:
  // The result value is measured with this precision.
  constexpr static int8_t kPrecisionBits = 8;

  BinarySearch(TCB0Delay& delay, TCA0_PWM& pwm, InputPin input)
      : delay_(delay),
        pwm_(pwm),
        input_(input),
        lower_(0),
        upper_((1 << kPrecisionBits) - 1) {
    SetPwm();
  }

  // Returns the measured return value, or -1 if not available yet.
  // TODO: Return `FixedPointFraction` instead.
  int_fast16_t OnInterrupt() {
    if (upper_ == lower_) {
      return lower_;
    }
    if (delay_.HasTriggered()) {
      if (input_.Read()) {
        lower_ = middle();
      } else {
        upper_ = middle() - 1;
      }
      SetPwm();
    }
    return -1;
  }

 private:
  void SetPwm() {
    // We shift 1 bit less so that the maximum value for PWV is 0.5 - at which
    // the signal at the base frequency is the strongest.
    constexpr static uint8_t kShift =
        FixedPointFraction<>::kFractionBits - kPrecisionBits - 1;
    pwm_.SetDutyCycle(FixedPointFraction<>(middle() << kShift));
    delay_.Start();
  }

  // As long as `upper_ > lower_`, the result is always `> _lower`.
  uint_fast16_t middle() const { return (lower_ + upper_ + 1) / 2; }

  TCB0Delay& delay_;
  TCA0_PWM& pwm_;
  InputPin input_;
  // A value at [lower_] is known to be 0.
  uint_fast16_t lower_;
  // A value at [upper_ + 1] is known to be 1.
  // It is assumed that [256] is always 1.
  uint_fast16_t upper_;
};

constexpr const TCA0_PWM::Config kLedPwmFreq(1.0);

struct __attribute__((packed)) TwiOutData {
  uint8_t channel1;
  uint8_t channel2;
};

static_assert(sizeof(TwiOutData) == 2, "TwiOutData != 2");

int main(void) {
  Sleep sleep(SLPCTRL_SMODE_IDLE_gc);
  Twi<TwiOutData> twi(kTwiAddress, TwiOutData{.channel1 = 42, .channel2 = 73});
  // Enable the TCA0 PB3 pin (WO0 alternate)
  PORTB.DIRSET = PIN3_bm;
  TCA0_PWM pwm(kLedPwmFreq);
  EVSYS.CHANNEL0 = EVSYS_CHANNEL0_TCA0_CMP0_LCMP0_gc;

  TCB0Delay delay(4, EVSYS_USER_CHANNEL0_gc);
  while (true) {
    BinarySearch search(delay, pwm, InputPin(PORTB, PIN1_bm));
    int_fast16_t signal;
    while ((signal = search.OnInterrupt()) < 0) {
      sleep.Start();
      twi.OnInterrupt();
    }
  }
  EVSYS.CHANNEL0 = EVSYS_CHANNEL0_OFF_gc;
}
