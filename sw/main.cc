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
#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/sleep.h>
#include <stdint.h>
#include <stdlib.h>
#include <util/atomic.h>

}  // extern "C"

// uint8_t EEMEM twi_address = 18;  // Randomly generated -
// https://xkcd.com/221/

template <typename T, typename U>
T exchange(T& ref, U new_value) {
  T result = ref;
  ref = static_cast<U&&>(new_value);
  return result;
}

class TCA0_PWM {
 public:
  constexpr static uint16_t kDutyCycleBits = 14;
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
    TCA0.SINGLE.CTRLD = 0;
    TCA0.SINGLE.EVCTRL = 0;
    TCA0.SINGLE.INTCTRL = 0;
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
    TCA0.SINGLE.CTRLESET = TCA_SINGLE_CMD_RESTART_gc;
  }
};

constexpr float TCA0_PWM::Config::kClkSelFreq[];

// Counts a given number of TCA0 cycles (CMP0) and then triggers an interrupt.
// Uses channels:
// (0) To pass events - pulses from TCA0 (PWM).
// (1) To trigger the delay immediately on construction.
class TCB0DelayFromTCA0 {
 public:
  explicit TCB0DelayFromTCA0(uint16_t count) {
    EVSYS.USERTCB0COUNT = kChannelClk;
    EVSYS.USERTCB0CAPT = kChannelStart;
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_TCA0_CMP0_LCMP0_gc;
    // See Section 22.3.3.1.7 in the manual.
    TCB0.EVCTRL = TCB_CAPTEI_bm;
    TCB0.CTRLB = TCB_CNTMODE_SINGLE_gc;
    HasTriggered();  // Clear any pending interrupts.
    TCB0.INTCTRL = TCB_CAPT_bm;
    TCB0_CCMP = count - 1;
    TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc;  // Enable last.
    EVSYS.SWEVENTA = kUserEvent;
  }
  ~TCB0DelayFromTCA0() {
    TCB0.CTRLA = 0;  // Disable.
    TCB0.EVCTRL = 0;
    EVSYS.CHANNEL0 = EVSYS_CHANNEL0_OFF_gc;
    EVSYS.USERTCB0COUNT = EVSYS_USER_OFF_gc;
  }

  bool IsRunning() { return TCB0.STATUS & TCB_RUN_bm; }
  // Returns whether the delay has been reached and the interrupt invoked.
  // Cleared by the call.
  bool HasTriggered() {
    return exchange(TCB0.INTFLAGS, TCB_CAPT_bm) & TCB_CAPT_bm;
  }

 private:
  constexpr static EVSYS_USER_t kChannelClk = EVSYS_USER_CHANNEL0_gc;
  constexpr static EVSYS_USER_t kChannelStart = EVSYS_USER_CHANNEL1_gc;
  constexpr static EVSYS_SWEVENTA_t kUserEvent = EVSYS_SWEVENTA_CH1_gc;
};

EMPTY_INTERRUPT(TCB0_INT_vect);

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

  Sleep sleep(SLPCTRL_SMODE_IDLE_gc);
  while (true) {
    {
      pwm.SetDutyCycle(7 << (TCA0_PWM::kDutyCycleBits - 3));
      TCB0DelayFromTCA0 delay(4);
      while (!delay.HasTriggered()) {
        sleep.Start();
      }
    }
    {
      pwm.SetDutyCycle(1 << (TCA0_PWM::kDutyCycleBits - 3));
      TCB0DelayFromTCA0 delay(4);
      while (!delay.HasTriggered()) {
        sleep.Start();
      }
    }
  }
}
