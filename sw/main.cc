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
#include <util/delay.h>

}  // extern "C"

// uint8_t EEMEM twi_address = 18;  // Randomly generated -
// https://xkcd.com/221/

class TCA0_PWM {
 public:
  TCA0_PWM(float /*freq*/, float /*duty_cycle*/)
      : prev_ctrla_(TCA0.SINGLE.CTRLA), prev_ctrlb_(TCA0.SINGLE.CTRLB) {
    TCA0.SINGLE.PER = 3255;
    TCA0.SINGLE.CMP0 = 3000;
    TCA0.SINGLE.CTRLB = TCA_SINGLE_WGMODE_SINGLESLOPE_gc | TCA_SINGLE_CMP0EN_bm;
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc | TCA_SINGLE_ENABLE_bm;
  }
  ~TCA0_PWM() { TCA0.SINGLE.CTRLA = prev_ctrla_; }

 private:
  const uint8_t prev_ctrla_;
  const uint8_t prev_ctrlb_;
};

int main(void) {
  // eeprom_read_byte(&twi_address);
  // Enable the TCA0 WO0 (PB0) pin.
  PORTB.DIRSET = PIN0_bm;
  TCA0_PWM pwm(1.0, 0.25);

  set_sleep_mode(SLEEP_MODE_IDLE);
  sleep_enable();
  while (true) {
    sleep_mode();
  }
}
