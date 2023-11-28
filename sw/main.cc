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
#include <util/atomic.h>

}  // extern "C"

#include "timer.h"

// uint8_t EEMEM twi_address = 18;  // Randomly generated -
// https://xkcd.com/221/

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

constexpr const TCA0_PWM::Config kLedPwmFreq(1.0);

int main(void) {
  // eeprom_read_byte(&twi_address);
  // Enable the TCA0 PA5 pin.
  PORTB.DIRSET = PIN0_bm;
  TCA0_PWM pwm(kLedPwmFreq);
  EVSYS.CHANNEL0 = EVSYS_CHANNEL0_TCA0_CMP0_LCMP0_gc;

  Sleep sleep(SLPCTRL_SMODE_IDLE_gc);
  while (true) {
    {
      pwm.SetDutyCycle(0.75);
      TCB0Delay delay(4, EVSYS_USER_CHANNEL0_gc);
      // while (!delay.HasTriggered()) {
      sleep.Start();
      //}
    }
    {
      pwm.SetDutyCycle(0.25);
      TCB0Delay delay(4, EVSYS_USER_CHANNEL0_gc);
      // while (!delay.HasTriggered()) {
      sleep.Start();
      //}
    }
  }
  EVSYS.CHANNEL0 = EVSYS_CHANNEL0_OFF_gc;
}
