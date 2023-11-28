#include "timer.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

constexpr float TCA0_PWM::Config::kClkSelFreq[];

TCA0_PWM::TCA0_PWM(Config freq) {
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

TCA0_PWM::~TCA0_PWM() {
  TCA0.SINGLE.CTRLA = 0;  // Disable completely.
}

TCB0Delay::TCB0Delay(uint16_t count, EVSYS_USER_t input_channel,
                     EVSYS_USER_t helper_channel) {
  EVSYS.USERTCB0COUNT = input_channel;
  EVSYS.USERTCB0CAPT = helper_channel;
  // See Section 22.3.3.1.7 in the manual.
  TCB0.EVCTRL = TCB_CAPTEI_bm;
  TCB0.CTRLB = TCB_CNTMODE_SINGLE_gc;
  (void)HasTriggered();  // Clear any pending interrupts.
  TCB0.INTCTRL = TCB_CAPT_bm;
  TCB0_CCMP = count - 1;
  TCB0.CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc;  // Enable last.
  // Trigger an event immediately.
  EVSYS.SWEVENTA =
      1 << (helper_channel - EVSYS_USER_CHANNEL0_gc + EVSYS_SWEVENTA_gp);
}
TCB0Delay::~TCB0Delay() {
  TCB0.CTRLA = 0;  // Disable.
  TCB0.EVCTRL = 0;
  EVSYS.USERTCB0COUNT = EVSYS_USER_OFF_gc;
}

EMPTY_INTERRUPT(TCB0_INT_vect);
