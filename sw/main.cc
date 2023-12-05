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
#include "twi_smbus.h"

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

struct Registers {
 public:
  void Snapshot() {}
  bool HasRegister(uint8_t reg) const { return ReadWord(reg).has_value(); }
  optional<int16_t> ReadWord(uint8_t reg) const {
    switch (reg) {
      case 0:
        return led1;
      case 1:
        return led2;
      default:
        return {};
    }
  }

  int16_t led1;
  int16_t led2;
};

using TwiRegisters = TwiClient<SMBusClient<Registers&>>;

class BinarySearch {
 public:
  using value_type = FixedPointFraction<int_fast16_t, 8>;

  BinarySearch(TCB0Delay& delay, TCA0_PWM& pwm, InputPin input)
      : delay_(delay),
        pwm_(pwm),
        input_(input),
        lower_(0),
        upper_(value_type(1.0f).fraction_bits - 1) {
    SetPwm();
  }

  // Returns the measured return value in [0..1], or a negative value if not
  // available yet.
  value_type OnInterrupt() {
    if (upper_ == lower_) {
      return value_type(lower_);
    }
    if (delay_.HasTriggered()) {
      if (input_.Read()) {
        lower_ = middle();
      } else {
        upper_ = middle() - 1;
      }
      SetPwm();
    }
    return value_type{-1};
  }

 private:
  void SetPwm() {
    // We shift 1 bit less so that the maximum value for PWM is 0.5 - at which
    // the signal at the base frequency is the strongest.
    constexpr static int_fast16_t kShift =
        FixedPointFraction<>::kFractionBits - value_type::kFractionBits - 1;
    static_assert(kShift >= 0, "Precision exceeds the PWM precision");
    pwm_.SetDutyCycle(FixedPointFraction<>(middle() << kShift));
    delay_.Start();
  }

  // As long as `upper_ > lower_`, the result is always `> _lower`.
  value_type::value_type middle() const { return (lower_ + upper_ + 1) / 2; }

  TCB0Delay& delay_;
  TCA0_PWM& pwm_;
  InputPin input_;
  // A value at [lower_] is known to be 0.
  value_type::value_type lower_;
  // A value at [upper_ + 1] is known to be 1.
  // It is assumed that [256] is always 1.
  value_type::value_type upper_;
};

BinarySearch::value_type BinarySearchLoop(TCA0_PWM& pwm, TCB0Delay& delay,
                                          TwiRegisters& twi, Sleep& sleep,
                                          InputPin opt_in) {
  BinarySearch search(delay, pwm, opt_in);
  BinarySearch::value_type signal(0);
  while ((signal = search.OnInterrupt()).fraction_bits < 0) {
    sleep.Start();
    twi.OnInterrupt();
  }
  return signal;
}

constexpr const TCA0_PWM::Config kLedPwmFreq(1.0);

int main(void) {
  Sleep sleep(SLPCTRL_SMODE_IDLE_gc);
  Registers regs;
  TwiRegisters twi(kTwiAddress, SMBusClient<Registers&>(regs));
  // Enable output for pins that provide GND to LEDs.
  // Invert so that a logical 1 turns the LED on (GND).
  PORTA.DIRSET = PIN5_bm | PIN6_bm;
  PORTA.PIN5CTRL = PORT_INVEN_bm;
  PORTA.PIN6CTRL = PORT_INVEN_bm;
  // Optical sensor input pin (inverted).
  const InputPin kOptIn(PORTB, PIN2_bm);
  PORTB.PIN2CTRL = PORT_INVEN_bm;
  // Enable the TCA0 PB3 pin (WO0 alternate)
  PORTB.DIRSET = PIN3_bm;
  TCA0_PWM pwm(kLedPwmFreq);
  EVSYS.CHANNEL0 = EVSYS_CHANNEL0_TCA0_CMP0_LCMP0_gc;

  TCB0Delay delay(4, EVSYS_USER_CHANNEL0_gc);
  while (true) {
    PORTA.OUTCLR = PIN6_bm;
    PORTA.OUTSET = PIN5_bm;
    regs.led1 = BinarySearchLoop(pwm, delay, twi, sleep, kOptIn)
                    .Convert<int16_t, 15>()
                    .fraction_bits;
    PORTA.OUTCLR = PIN5_bm;
    PORTA.OUTSET = PIN6_bm;
    regs.led2 = BinarySearchLoop(pwm, delay, twi, sleep, kOptIn)
                    .Convert<int16_t, 15>()
                    .fraction_bits;
  }
  EVSYS.CHANNEL0 = EVSYS_CHANNEL0_OFF_gc;
}
