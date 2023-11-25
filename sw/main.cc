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

#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdlib.h>

}  // extern "C"

// uint8_t EEMEM twi_address = 18;  // Randomly generated -
// https://xkcd.com/221/

class TCA0_PWM {
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
  TCA0_PWM(Config freq) {
    // See Section 21.5.1 in the manual.
    PORTMUX.TCAROUTEA = 0;
    TCA0.SINGLE.PER = freq.per;
    TCA0.SINGLE.CMP0 = 0;  // Duty cycle.
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;
    // Enable last.
    TCA0.SINGLE.CTRLA =
        ((freq.clkSel << TCA_SINGLE_CLKSEL_gp) & TCA_SINGLE_CLKSEL_gm) |
        TCA_SINGLE_ENABLE_bm;
  }
  ~TCA0_PWM() {
    TCA0.SINGLE.CTRLA = 0;  // Disable completely.
  }

  // `duty_cycle` from 0 (static low) to 2^14 (static high).
  void SetDutyCycle(uint16_t duty_cycle_14bit) {
    TCA0.SINGLE.CMP0 = static_cast<uint16_t>(
        ((long{TCA0.SINGLE.PER} + 1) * duty_cycle_14bit) >> 14);
  }
};

constexpr float TCA0_PWM::Config::kClkSelFreq[];

constexpr const TCA0_PWM::Config kLedPwmFreq(1.0);

int main(void) {
  // eeprom_read_byte(&twi_address);
  // Enable the TCA0 PA5 pin.
  PORTB.DIRSET = PIN0_bm;
  TCA0_PWM pwm(kLedPwmFreq);
  pwm.SetDutyCycle(3 << 12);

  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  while (true) {
    sleep_mode();
  }
}
