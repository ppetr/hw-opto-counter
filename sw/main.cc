#include <avr/eeprom.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

uint8_t EEMEM twi_address = 18; // Randomly generated - https://xkcd.com/221/

int main(void) {
  eeprom_write_byte(&twi_address, 42);
  _delay_ms(1000);
  return 0;
}
