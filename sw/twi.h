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

#ifndef _TWI_H
#define _TWI_H

extern "C" {

#include <avr/io.h>
#include <stdint.h>
#include <util/twi.h>

}  // extern "C"

#include "util.h"

// See also https://www.nongnu.org/avr-libc/examples/twitest/twitest.c

template <typename ReadData>
class Twi {
 public:
  using value_type = ReadData;

  Twi(uint8_t address, const ReadData& read_data)
      : read_data_(read_data), read_snapshot_(read_data), read_index_(0) {
    TWI0.CTRLA = TWI_SDASETUP_4CYC_gc | TWI_SDAHOLD_500NS_gc;
    TWI0.SADDR = address << 1;
    TWI0.SADDRMASK = 0;
    // Standard or regular fast mode.
    TWI0.SCTRLA = TWI_ENABLE_bm | TWI_DIEN_bm | TWI_PIEN_bm | TWI_APIEN_bm;
  }
  ~Twi() { TWI0.SCTRLA = 0; }
  Twi(const Twi&) = delete;
  Twi& operator=(const Twi&) = delete;

  void Publish(const ReadData& read_data) { read_data_ = read_data; }

  void OnInterrupt() { TWI0.SCTRLB = OnInterrupt(TWI0.SSTATUS); }

 private:
  // Returns the value to be written into `SCTRLB`.
  uint8_t OnInterrupt(uint8_t status) {
    // Writing a TWI_SCMD... command to SCTRLB clears TWI_DIF and TWI_APIF.
    constexpr static uint8_t kTwiDirHostRead = TWI_DIR_bm;
    if (status & TWI_BUSERR_bm) {
      // Clear the flag and wait for the protocol to restart.
      TWI0.SSTATUS = TWI_BUSERR_bm;
      return TWI_SCMD_NOACT_gc;
    } else if (status &
               TWI_COLL_bm) {  // The flag will be cleared automatically.
      // TODO: Reset the internal state.
      return TWI_SCMD_NOACT_gc;
    } else if (status & TWI_APIF_bm) {  // Address or stop interrupt.
      if ((status & TWI_AP_bm) == TWI_AP_ADR_gc) {  // Address.
        read_snapshot_ = read_data_;
        read_index_ = 0;
        return TWI_ACKACT_ACK_gc | TWI_SCMD_RESPONSE_gc;
      } else {  // Stop.
        return TWI_ACKACT_ACK_gc | TWI_SCMD_COMPTRANS_gc;
      }
    } else if (status & TWI_DIF_bm) {  // Data interrupt.
      if ((status & TWI_DIR_bm) == kTwiDirHostRead) {
        if (read_index_ < sizeof(read_snapshot_)) {
          TWI0.SDATA =
              reinterpret_cast<const uint8_t*>(&read_snapshot_)[read_index_++];
          return TWI_ACKACT_ACK_gc | TWI_SCMD_RESPONSE_gc;
        } else {
          return TWI_ACKACT_NACK_gc | TWI_SCMD_COMPTRANS_gc;
        }
      } else {  // Host write.
        // Swallow up any incoming data.
        (void)TWI0.SDATA;
        // TODO: Use this to receive a register number to read from.
        return TWI_ACKACT_ACK_gc | TWI_SCMD_RESPONSE_gc;
      }
    } else {
      return TWI_SCMD_NOACT_gc;
    }
  }

  ReadData read_data_;
  // Currently being read by `OnInterrupt`.
  ReadData read_snapshot_;
  size_t read_index_;
};

EMPTY_INTERRUPT(TWI0_TWIS_vect);

#endif  // _TWI_H
