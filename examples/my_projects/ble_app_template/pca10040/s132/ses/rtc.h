#ifndef RTC_H
#define RTC_H


#include "nrf_drv_twi.h"
#include "pin_definitions.h"
#include <stddef.h>
#include <stdint.h>


#define RTC_ADDR 0x52U
#define TWI_INSTANCE_ID 0


extern const nrf_drv_twi_t m_twi;

#define RTC_REG_SECONDS 0x00U
#define RTC_BCD_MASK 0x7FU

#define RTC_REG_STATUS 0x0E          /* correct Status register   */
#define RTC_REG_CTRL1  0x0F          /* Control-1                 */
#define RTC_REG_CTRL2  0x10          /* Control-2                 */
//#define RTC_REG_CTRL1 0x10
//#define RTC_REG_CTRL2 0x11
#define RTC_REG_ALARM_SEC    0x06
#define RTC_REG_ALARM_MIN    0x07
#define RTC_REG_ALARM_HOUR   0x08
#define RTC_REG_ALARM_WDAY   0x09
//#define RTC_REG_STATUS 0x0F

//#define RTC_CTRL2_AIE 0x02
#define RTC_CTRL2_AIE  0x08
#define RTC_CTRL1_ARST (1 << 2) // Auto reset
#define RTC_STATUS_AF 0x01

static inline uint8_t dec2bcd(uint8_t d)
{
    return (d / 10U << 4) | (d % 10U);
}

static inline uint8_t bcd2dec(uint8_t b) { return (b >> 4) * 10U + (b & 0x0FU); }

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

typedef struct
{
    uint8_t min;  // Minutes (0-59)
    uint8_t hour; // Hours (0-23)
    uint8_t wday; // Weekday (0-6), bit 7 = enable
} rtc_alarm_t;

ret_code_t rtc_write(uint8_t reg, uint8_t const *p_data, size_t len);

ret_code_t rtc_read(uint8_t reg, uint8_t *p_data, size_t len);

ret_code_t rtc_set_datetime(rtc_datetime_t const *dt);

ret_code_t rtc_get_datetime(rtc_datetime_t *dt);

void print_datetime(char const *prefix, rtc_datetime_t const *dt);

void rtc_init(uint8_t);

ret_code_t rtc_set_alarm(rtc_alarm_t const *alarm);

ret_code_t rtc_set_alarm_time(uint8_t hour, uint8_t min);

bool rtc_check_alarm_flag(void);

ret_code_t rtc_set_alarm_flag(void);

ret_code_t rtc_get_alarm(rtc_alarm_t *alarm);

ret_code_t rtc_disable_alarm_interrupt(void);

void clear_alarm_flag(void);

#endif