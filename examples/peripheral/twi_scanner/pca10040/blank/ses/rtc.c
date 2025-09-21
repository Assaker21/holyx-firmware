#include "app_error.h"
#include "boards.h"
#include "nrf_delay.h"
#include <stdio.h>
#include <string.h>
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "rtc.h"

const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);


ret_code_t rtc_write(uint8_t reg, uint8_t const * p_data, size_t len)
{
    uint8_t buf[1 + 16];
    buf[0] = reg;
    memcpy(&buf[1], p_data, len);
    return nrf_drv_twi_tx(&m_twi, RTC_ADDR, buf, len + 1, false);
}

ret_code_t rtc_read(uint8_t reg, uint8_t * p_data, size_t len)
{
    ret_code_t err;
    err = nrf_drv_twi_tx(&m_twi, RTC_ADDR, &reg, 1, true);
    if (err != NRF_SUCCESS) return err;
    return nrf_drv_twi_rx(&m_twi, RTC_ADDR, p_data, len);
}

ret_code_t rtc_set_datetime(rtc_datetime_t const * dt)
{
    uint8_t buf[7] = {
        dec2bcd(dt->sec),
        dec2bcd(dt->min),
        dec2bcd(dt->hour),
        dec2bcd(dt->wday),
        dec2bcd(dt->mday),
        dec2bcd(dt->month),
        dec2bcd(dt->year)
    };
    return rtc_write(RTC_REG_SECONDS, buf, sizeof(buf));
}

ret_code_t rtc_get_datetime(rtc_datetime_t * dt)
{
    uint8_t buf[7];
    ret_code_t err = rtc_read(RTC_REG_SECONDS, buf, sizeof(buf));
    if (err != NRF_SUCCESS) return err;

    dt->sec   = bcd2dec(buf[0] & RTC_BCD_MASK);
    dt->min   = bcd2dec(buf[1] & RTC_BCD_MASK);
    dt->hour  = bcd2dec(buf[2] & 0x3F);
    dt->wday  = bcd2dec(buf[3] & 0x07);
    dt->mday  = bcd2dec(buf[4] & 0x3F);
    dt->month = bcd2dec(buf[5] & 0x1F);
    dt->year  = bcd2dec(buf[6]);
    return NRF_SUCCESS;
}

void print_datetime(char const * prefix, rtc_datetime_t const * dt)
{
    printf("%s %02u-%02u-20%02u %02u:%02u:%02u (wd=%u)\n",
           prefix,
           dt->mday, dt->month, dt->year,
           dt->hour, dt->min, dt->sec,
           dt->wday);
}

void twi_init(void)
{
    const nrf_drv_twi_config_t cfg = {
        .scl                = ARDUINO_SCL_PIN,
        .sda                = ARDUINO_SDA_PIN,
        .frequency          = NRF_DRV_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init     = false,
    };
    APP_ERROR_CHECK(nrf_drv_twi_init(&m_twi, &cfg, NULL, NULL));
    nrf_drv_twi_enable(&m_twi);
}