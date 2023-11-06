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
#include <stdint.h>
#include <util/delay.h>

}  // extern "C"

uint8_t EEMEM twi_address = 18;  // Randomly generated - https://xkcd.com/221/

int main(void) {
  eeprom_read_byte(&twi_address);
  _delay_ms(1000);
  return 0;
}
