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

#ifndef _TIMER_H
#define _TIMER_H

#include <avr/io.h>
#include <stdint.h>

#include "util.h"

class TCA0_PWM final {
 public:
  struct Config {
    // Finds which divider value will fit in the 16-bit counter with maximum
    // precision. A `constexpr` constructor allows to do all floating point
    // computations at compile time.
    constexpr explicit Config(float freq)
        : clkSel(ClkSelFor(freq)),
          per(static_cast<uint16_t>(kClkSelFreq[clkSel] / freq - 1)) {}

    // The clock prescaler selection 0-7.
    uint8_t clkSel;
    // The TOP counter value.
    uint16_t per;

   private:
    constexpr static uint8_t ClkSelFor(float freq) {
      uint8_t clkSel = 0;
      for (; clkSel < 8; clkSel++) {
        if (kClkSelFreq[clkSel] / 65536.0 < freq) {
          break;
        }
      }
      return clkSel;
    }
    constexpr static float kClkSelFreq[] = {
        float{F_CPU} / 1.0,   float{F_CPU} / 2.0,   float{F_CPU} / 4.0,
        float{F_CPU} / 8.0,   float{F_CPU} / 16.0,  float{F_CPU} / 64.0,
        float{F_CPU} / 256.0, float{F_CPU} / 1024.0};
  };

  // Sets up the PWM, but with 0 duty cycle.
  TCA0_PWM(Config freq);
  TCA0_PWM(const TCA0_PWM&) = delete;
  TCA0_PWM& operator=(const TCA0_PWM&) = delete;
  ~TCA0_PWM();

  // `duty_cycle` within [0..1].
  void SetDutyCycle(FixedPointFraction<> duty_cycle) {
    TCA0.SINGLE.CMP0 = static_cast<uint16_t>(
        ((long{TCA0.SINGLE.PER} + 1) * long{duty_cycle.fraction_bits}) >>
        duty_cycle.kFractionBits);
    TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_RESTART_gc;
  }
};

// Counts a given number of input event cycles and then triggers an interrupt.
// Uses channels:
// `input_channel`: An event in this channel triggers a delayed pulse. This is
//     exactly what is done by `Start`.
// `helper_channel`: To pass events - pulses from TCA0 (PWM).
class TCB0Delay {
 public:
  explicit TCB0Delay(uint16_t count, EVSYS_USER_t input_channel,
                     EVSYS_USER_t helper_channel = EVSYS_USER_CHANNEL5_gc);
  TCB0Delay(const TCB0Delay&) = delete;
  TCB0Delay& operator=(const TCB0Delay&) = delete;
  ~TCB0Delay();

  void Start() {
    (void)HasTriggered();  // Clear any pending interrupts.
    TCB0.CNT = 0;
    EVSYS.SWEVENTA = trigger_event_;
  }

  bool IsRunning() const { return TCB0.STATUS & TCB_RUN_bm; }
  // Returns whether the delay has been reached and the interrupt invoked.
  // Cleared by the call.
  bool HasTriggered() {
    return exchange(TCB0.INTFLAGS, TCB_CAPT_bm) & TCB_CAPT_bm;
  }

 private:
  const EVSYS_SWEVENTA_t trigger_event_;
};

#endif  // _TIMER_H
