#ifndef RTC_H 
#define RTC_H

#include <stdint.h>
#include <stddef.h>
#include "nrf_drv_twi.h"

#define RTC_ADDR          0x52U
#define TWI_INSTANCE_ID      0

extern const nrf_drv_twi_t m_twi;

#define RTC_REG_SECONDS   0x00U
#define RTC_BCD_MASK      0x7FU

static inline uint8_t dec2bcd(uint8_t d)       { return (d / 10U << 4) | (d % 10U); }
static inline uint8_t bcd2dec(uint8_t b)       { return (b >> 4) * 10U + (b & 0x0FU); }

typedef struct
{
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t wday;
    uint8_t mday;
    uint8_t month;
    uint8_t year;
} rtc_datetime_t;


ret_code_t rtc_write(uint8_t reg, uint8_t const * p_data, size_t len);

ret_code_t rtc_read (uint8_t reg, uint8_t * p_data, size_t len);

ret_code_t rtc_set_datetime(rtc_datetime_t const * dt);

ret_code_t rtc_get_datetime(rtc_datetime_t * dt);

void print_datetime(char const * prefix, rtc_datetime_t const * dt);

void twi_init(void);
#endif
