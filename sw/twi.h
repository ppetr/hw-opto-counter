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

template <typename IO>
class TwiClient {
 public:
  using value_type = IO;

  struct Config {
    uint8_t address;
    TWI_SDASETUP_t sda_setup = TWI_SDASETUP_4CYC_gc;
    TWI_SDAHOLD_t bus_timeout = TWI_SDAHOLD_500NS_gc;
  };

  TwiClient(uint8_t address, IO io)
      : TwiClient(Config{.address = address}, move(io)) {}
  TwiClient(Config config, IO io) : io_(move(io)), in_transaction_(false) {
    TWI0.CTRLA = config.sda_setup | config.bus_timeout;
    TWI0.SADDR = config.address << 1;
    TWI0.SADDRMASK = 0;
    // Standard or regular fast mode.
    TWI0.SCTRLA = TWI_ENABLE_bm | TWI_DIEN_bm | TWI_PIEN_bm | TWI_APIEN_bm;
  }
  ~TwiClient() { TWI0.SCTRLA = 0; }
  TwiClient(const TwiClient&) = delete;
  TwiClient& operator=(const TwiClient&) = delete;

  void OnInterrupt() { TWI0.SCTRLB = OnInterrupt(TWI0.SSTATUS); }

 private:
  // Returns the value to be written into `SCTRLB`.
  uint8_t OnInterrupt(const uint8_t status) {
    // Writing a TWI_SCMD... command to SCTRLB clears TWI_DIF and TWI_APIF.
    constexpr static uint8_t kTwiDirHostRead = TWI_DIR_bm;
    if (status & TWI_BUSERR_bm) {
      if (exchange(in_transaction_, false)) {
        io_.TransactionAbort();
      }
      TWI0.SSTATUS = TWI_BUSERR_bm;  // Clear the flag.
      return TWI_SCMD_NOACT_gc;
    } else if (status & TWI_COLL_bm) {
      if (exchange(in_transaction_, false)) {
        io_.TransactionAbort();
      }
      return TWI_SCMD_NOACT_gc;  // The flag will be cleared automatically.
    } else if (status & TWI_APIF_bm) {  // Address or stop interrupt.
      if ((status & TWI_AP_bm) == TWI_AP_STOP_gc) {
        if (exchange(in_transaction_, false)) {
          io_.TransactionStop();
        }
        return TWI_ACKACT_ACK_gc | TWI_SCMD_COMPTRANS_gc;
      } else {  // Address interrupt.
        if (!exchange(in_transaction_, true)) {
          io_.TransactionStart();
        }
        if ((status & TWI_DIR_bm) == kTwiDirHostRead) {
          return ActAck(io_.ReadStart()) | TWI_SCMD_RESPONSE_gc;
        } else {  // Host write.
          return ActAck(io_.WriteStart()) | TWI_SCMD_RESPONSE_gc;
        }
      }
    } else if (status & TWI_DIF_bm) {  // Data interrupt.
      if ((status & TWI_DIR_bm) == kTwiDirHostRead) {
        int_fast16_t data = io_.Read();
        if (data >= 0) {
          TWI0.SDATA = static_cast<uint8_t>(data);
          return TWI_ACKACT_ACK_gc | TWI_SCMD_RESPONSE_gc;
        } else {
          return TWI_ACKACT_NACK_gc | TWI_SCMD_RESPONSE_gc;
        }
      } else {  // Host write.
        return ActAck(io_.Write(TWI0.SDATA)) | TWI_SCMD_RESPONSE_gc;
      }
    } else {
      return TWI_SCMD_NOACT_gc;
    }
  }

  static constexpr TWI_ACKACT_t ActAck(bool ack) {
    return ack ? TWI_ACKACT_ACK_gc : TWI_ACKACT_NACK_gc;
  }

  IO io_;
  bool in_transaction_;
};

EMPTY_INTERRUPT(TWI0_TWIS_vect);

#endif  // _TWI_H
