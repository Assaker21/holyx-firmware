#ifndef EEPROM_H
#define EEPROM_H

#include <stdint.h>
#include <stdbool.h>
#include "sdk_errors.h"

#define EEPROM_FILE_ID  0x1111
#define EEPROM_REC_KEY  0x2222

ret_code_t eeprom_init(void);
ret_code_t eeprom_write(const char* data, uint16_t length);
ret_code_t eeprom_read(char* buffer, uint16_t* length);
ret_code_t eeprom_clear(void);

#endif // EEPROM_H
