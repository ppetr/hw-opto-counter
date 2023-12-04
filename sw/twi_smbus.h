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

#ifndef _TWI_SMBUS_H
#define _TWI_SMBUS_H

extern "C" {

#include <stdint.h>

}  // extern "C"

#include "util.h"

// Implements a subset of the SMBus protocol on top of `TwiClient`.
// At the moment it only supports reading word (2-byte) registers.
// See https://docs.kernel.org/i2c/smbus-protocol.html for details.
// TODO: Add PEC
// (https://docs.kernel.org/i2c/smbus-protocol.html#packet-error-checking-pec).
template <typename Registers>
class SMBusClient {
 public:
  explicit SMBusClient(Registers registers)
      : registers_(forward<Registers>(registers)) {}

  // Each transaction is demarcated by start-stop (or start-abort).
  void TransactionStart() {
    registers_.Snapshot();
    command_.reset();
  }
  void TransactionAbort() { TransactionStop(); }
  void TransactionStop() {}
  // Called to acknowledge start of a write block.
  bool WriteStart() {
    index_ = 0;
    return true;
  }
  // Called to acknowledge the reception of a byte.
  bool Write(uint8_t data) {
    if (!command_) {
      command_.emplace(data);
      index_ = 0;
      return registers_.HasRegister(data);
    } else {  // No register writes supported currently.
      return false;
    }
  }
  // Called to acknowledge the start of a read block.
  bool ReadStart() {
    if (!command_.has_value()) {
      // Allow (and ignore) a read without a command for a Quick command
      // (assuming the transaction ends straight away).
      return true;
    }
    index_ = 0;
    optional<int16_t> data = registers_.ReadWord(*command_);
    if (!data.has_value()) {
      index_ = sizeof(buffer_);
      return false;
    }
    buffer_[0] = static_cast<uint8_t>(*data & 0xff);  // Low byte first.
    buffer_[1] = static_cast<uint8_t>((*data >> 8) & 0xff);
    return true;
  }
  // Called to return the next value to be passed to the host.
  // Returning `-1` signals that there is no more data available.
  optional<uint8_t> Read() {
    if (index_ < static_cast<intptr_t>(sizeof(buffer_))) {
      return buffer_[index_++];
    } else {
      return {};
    }
  }

 private:
  Registers registers_;
  optional<uint8_t> command_;
  intptr_t index_;
  uint8_t buffer_[2];
};

#endif  // _TWI_SMBUS_H
