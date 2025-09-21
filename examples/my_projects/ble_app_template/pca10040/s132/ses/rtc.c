#include "rtc.h"
#include "app_error.h"
#include "boards.h"
#include "nrf_delay.h"
#include "nrf_drv_twi.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include <stdio.h>
#include <string.h>

#define RTC_REG_EVCTRL 0x0D
//#define RTC_REG_STATUS   0x0F
//#define RTC_REG_CTRL2    0x10

#define RTC_CTRL2_EIE (1 << 2)    // Event Interrupt Enable
#define RTC_STATUS_EVF (1 << 3)   // Event Flag
#define RTC_EVCTRL_EVIN (1 << 0)  // Enable external event input
#define RTC_EVCTRL_EVD (1 << 1)   // Edge detection
#define RTC_EVCTRL_EVPOL (1 << 2) // Polarity (1 = High)
#define RTC_EVCTRL_EVIE (1 << 3)  // Event Interrupt Enable
//#define RTC_STATUS_AF    (1 << 0)

const nrf_drv_twi_t m_twi = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID);

ret_code_t rtc_enable_event_interrupt_high(void)
{
    ret_code_t err;
    uint8_t reg;

    // 1. Clear any old event flag
    err = rtc_read(RTC_REG_STATUS, &reg, 1);
    if (err != NRF_SUCCESS)
        return err;
    reg &= ~RTC_STATUS_EVF;
    err = rtc_write(RTC_REG_STATUS, &reg, 1);
    if (err != NRF_SUCCESS)
        return err;

    // 2. Configure EVCTRL: enable event, high polarity, level detect, interrupt
    uint8_t evctrl = RTC_EVCTRL_EVIN | RTC_EVCTRL_EVIE | RTC_EVCTRL_EVPOL;
    // (use RTC_EVCTRL_EVD if you want edge detect instead of level)
    err = rtc_write(RTC_REG_EVCTRL, &evctrl, 1);
    if (err != NRF_SUCCESS)
        return err;

    // 3. Enable Event Interrupt Output in CTRL2
    err = rtc_read(RTC_REG_CTRL2, &reg, 1);
    if (err != NRF_SUCCESS)
        return err;
    reg |= RTC_CTRL2_EIE;
    return rtc_write(RTC_REG_CTRL2, &reg, 1);
}

ret_code_t rtc_write(uint8_t reg, uint8_t const *p_data, size_t len)
{
    uint8_t buf[1 + 16];
    buf[0] = reg;
    memcpy(&buf[1], p_data, len);
    return nrf_drv_twi_tx(&m_twi, RTC_ADDR, buf, len + 1, false);
}

ret_code_t rtc_read(uint8_t reg, uint8_t *p_data, size_t len)
{
    ret_code_t err;
    err = nrf_drv_twi_tx(&m_twi, RTC_ADDR, &reg, 1, true);
    if (err != NRF_SUCCESS)
        return err;
    return nrf_drv_twi_rx(&m_twi, RTC_ADDR, p_data, len);
}

ret_code_t rtc_set_datetime(rtc_datetime_t const *dt)
{
    uint8_t buf[7] = {
        dec2bcd(dt->sec),
        dec2bcd(dt->min),
        dec2bcd(dt->hour),
        dec2bcd(dt->wday),
        dec2bcd(dt->mday),
        dec2bcd(dt->month),
        dec2bcd(dt->year)};
    return rtc_write(RTC_REG_SECONDS, buf, sizeof(buf));
}

ret_code_t rtc_get_datetime(rtc_datetime_t *dt)
{
    uint8_t buf[7];
    ret_code_t err = rtc_read(RTC_REG_SECONDS, buf, sizeof(buf));
    if (err != NRF_SUCCESS)
        return err;

    dt->sec = bcd2dec(buf[0] & RTC_BCD_MASK);
    dt->min = bcd2dec(buf[1] & RTC_BCD_MASK);
    dt->hour = bcd2dec(buf[2] & 0x3F);
    dt->wday = bcd2dec(buf[3] & 0x07);
    dt->mday = bcd2dec(buf[4] & 0x3F);
    dt->month = bcd2dec(buf[5] & 0x1F);
    dt->year = bcd2dec(buf[6]);
    return NRF_SUCCESS;
}

void print_datetime(char const *prefix, rtc_datetime_t const *dt)
{
    printf("%s %02u-%02u-20%02u %02u:%02u:%02u (wd=%u)\n",
        prefix,
        dt->mday, dt->month, dt->year,
        dt->hour, dt->min, dt->sec,
        dt->wday);
}

ret_code_t rtc_enable_alarm_interrupt(void)
{
    ret_code_t err;
    uint8_t ctrl2;

    // Read current CTRL2 register
    err = rtc_read(RTC_REG_CTRL2, &ctrl2, 1);
    printf("RTC: READ? \n\r");
    if (err != NRF_SUCCESS)
    {
        printf("RTC: READ ERR \n\r");
        return err;
    }
    printf("RTC: READ DONE \n\r");
    // Set Alarm Interrupt Enable bit (AIE)
    ctrl2 |= RTC_CTRL2_AIE;

    // Write back to CTRL2
    printf("RTC: WRITE? \n\r");
    return rtc_write(RTC_REG_CTRL2, &ctrl2, 1);
}
static ret_code_t rtc_disable_alarm_irq(void)
{
    uint8_t c2;
    VERIFY_SUCCESS(rtc_read(RTC_REG_CTRL2, &c2, 1));
    c2 &= ~RTC_CTRL2_AIE; // bit-3 = 0
    return rtc_write(RTC_REG_CTRL2, &c2, 1);
}

/* clear *all* interrupt flags that share INT */
static ret_code_t rtc_clear_flags(void)
{
    uint8_t st;
    VERIFY_SUCCESS(rtc_read(RTC_REG_STATUS, &st, 1));
    st &= ~(0x0F); // AF | TF | UF | EVF ? 0
    return rtc_write(RTC_REG_STATUS, &st, 1);
}
void rtc_init(uint8_t setup_rtc)
{
    const nrf_drv_twi_config_t cfg = {
        .scl = RTC_SCL_PIN,
        .sda = RTC_SDA_PIN,
        .frequency = NRF_DRV_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false,
    };
    printf("RTC: config created. \n\r");
    APP_ERROR_CHECK(nrf_drv_twi_init(&m_twi, &cfg, NULL, NULL));
    printf("RTC: TWI init \n\r");
    nrf_drv_twi_enable(&m_twi);
    printf("RTC: TWI enabled \n\r");

    if (setup_rtc)
    {
        APP_ERROR_CHECK(rtc_enable_alarm_interrupt());
        printf("RTC: enabled alarm interrupt \n\r");
        //APP_ERROR_CHECK(rtc_enable_event_interrupt_high());
        //printf("RTC: enabled event interrupt \n\r");
        // rtc_disable_alarm_irq();   // 1. stop INT from reacting
        // rtc_clear_flags();         // 2. drop AF left from POR
    }
}

ret_code_t rtc_clear_af(void)
{
    uint8_t st;
    VERIFY_SUCCESS(rtc_read(RTC_REG_STATUS, &st, 1));
    st &= ~RTC_STATUS_AF; // write ‘0’ to bit-0 clears it
    return rtc_write(RTC_REG_STATUS, &st, 1);
}

ret_code_t rtc_enable_alarm_irq(void)
{
    uint8_t ctrl2;
    VERIFY_SUCCESS(rtc_read(RTC_REG_CTRL2, &ctrl2, 1));
    ctrl2 |= RTC_CTRL2_AIE; // bit-3 = 1  ? enable INT for alarm
    return rtc_write(RTC_REG_CTRL2, &ctrl2, 1);
}

ret_code_t rtc_set_alarm(rtc_alarm_t const *alarm)
{
    rtc_disable_alarm_irq(); // 1. silence INT
    uint8_t buf[4];

    buf[0] = 0x80;                       // Seconds disabled
    buf[1] = dec2bcd(alarm->min) & 0x7F; // Minutes enabled
    buf[2] = dec2bcd(alarm->hour) & 0x7F;

    if (alarm->wday == 0)
    {
        buf[3] = 0x80; // Weekday/Date disabled
    }
    else
    {
        buf[3] = dec2bcd(alarm->wday) & 0x7F; // Weekday enabled
    }

    ret_code_t error_code = rtc_write(RTC_REG_ALARM_SEC, buf, sizeof(buf));

    rtc_enable_alarm_irq();

    rtc_clear_flags(); // 2. be sure AF=0

    return error_code;
}

ret_code_t rtc_set_alarm_time(uint8_t hour, uint8_t min)
{
    rtc_disable_alarm_irq(); // 1. silence INT
    rtc_clear_flags();       // 2. be sure AF=0
    // RV-3028-C3 alarm registers: Seconds(0x06), Minutes(0x07), Hours(0x08), Weekday/Date(0x09)
    // Bit 7 = 0 means ENABLED, Bit 7 = 1 means DISABLED
    uint8_t buf[4];
    buf[0] = 0x80;                 // Seconds disabled (bit 7 = 1)
    buf[1] = dec2bcd(min) & 0x7F;  // Minutes enabled (bit 7 = 0)
    buf[2] = dec2bcd(hour) & 0x7F; // Hours enabled (bit 7 = 0)
    buf[3] = 0x80;                 // Weekday/Date disabled (bit 7 = 1)

    ret_code_t error_code = rtc_write(RTC_REG_ALARM_SEC, buf, sizeof(buf));

    rtc_enable_alarm_irq();

    return error_code;

    // rtc_disable_alarm_irq();   // 1. silence INT
    //     rtc_clear_flags();         // 2. be sure AF=0
    //     // RV-3028-C3 alarm registers: Seconds(0x06), Minutes(0x07), Hours(0x08), Weekday/Date(0x09)
    //     // Bit 7 = 0 means ENABLED, Bit 7 = 1 means DISABLED
    //     uint8_t buf[4];
    //     buf[0] = 0x80;                 // Seconds disabled (bit 7 = 1)
    //     buf[1] = dec2bcd(min) & 0x7F;  // Minutes enabled (bit 7 = 0)
    //     buf[2] = dec2bcd(hour) & 0x7F; // Hours enabled (bit 7 = 0)
    //     buf[3] = 0x80;                 // Weekday/Date disabled (bit 7 = 1)

    //    ret_code_t error_code = rtc_write(RTC_REG_ALARM_SEC, buf, sizeof(buf));
    //    printf("Set RTC ALARM succesfully to %d:%d\n\r", hour, min);

    //    //nrf_delay_ms(5000);
    //    // rtc_disable_alarm_irq(); // 1. silence INT
    //    // rtc_enable_alarm_irq();
    //    rtc_enable_alarm_irq();

    //    //rtc_clear_flags(); // 2. be sure AF=0

    //    return error_code;
}

bool rtc_check_alarm_flag(void)
{
    uint8_t status;
    ret_code_t err = rtc_read(RTC_REG_STATUS, &status, 1);
    if (err != NRF_SUCCESS)
        return false;

    if (status & RTC_STATUS_AF)
    {
        // Clear alarm flag by writing 0 to the AF bit
        // status &= ~RTC_STATUS_AF;
        // rtc_write(RTC_REG_STATUS, &status, 1);
        return true;
    }
    return false;
}

void clear_alarm_flag()
{
    uint8_t status;
    rtc_read(RTC_REG_STATUS, &status, 1);
    status &= ~RTC_STATUS_AF;
    rtc_write(RTC_REG_STATUS, &status, 1);
}

ret_code_t rtc_set_alarm_flag(void)
{
    ret_code_t err;
    rtc_datetime_t now_dt;
    rtc_alarm_t new_alarm;

    // 1. Get the current date and time
    err = rtc_get_datetime(&now_dt);
    if (err != NRF_SUCCESS)
    {
        return err;
    }

    err = rtc_disable_alarm_irq();
    if (err != NRF_SUCCESS)
    {
        return err;
    }
    err = rtc_clear_flags();
    if (err != NRF_SUCCESS)
    {
        return err;
    }

    uint8_t current_sec = bcd2dec(now_dt.sec);
    uint8_t new_sec = (current_sec + 5) % 60;
    uint8_t new_min = now_dt.min;
    if (current_sec >= 55)
    {
        new_min = (now_dt.min + 1) % 60;
    }

    uint8_t alarm_buf[4];
    alarm_buf[0] = dec2bcd(new_sec) & 0x7F; // Seconds alarm disabled
    alarm_buf[1] = dec2bcd(new_min) & 0x7F; // Minutes enabled
    alarm_buf[2] = 0x80;                    // Hours disabled
    alarm_buf[3] = 0x80;                    // Weekday/Date disabled

    err = rtc_write(RTC_REG_ALARM_SEC, alarm_buf, sizeof(alarm_buf));
    if (err != NRF_SUCCESS)
    {
        return err;
    }

    err = rtc_enable_alarm_irq();
    if (err != NRF_SUCCESS)
    {
        return err;
    }

    return NRF_SUCCESS;
}

ret_code_t rtc_get_alarm(rtc_alarm_t *alarm)
{
    uint8_t buf[4];
    ret_code_t err = rtc_read(RTC_REG_ALARM_SEC, buf, sizeof(buf));
    if (err != NRF_SUCCESS)
        return err;

    // Only read values if they are enabled (bit 7 = 0)
    alarm->min = (buf[1] & 0x80) ? 0 : bcd2dec(buf[1] & 0x7F);
    alarm->hour = (buf[2] & 0x80) ? 0 : bcd2dec(buf[2] & 0x7F);
    alarm->wday = (buf[3] & 0x80) ? 0 : bcd2dec(buf[3] & 0x7F);

    return NRF_SUCCESS;
}

ret_code_t rtc_disable_alarm_interrupt(void)
{
    uint8_t ctrl2;
    ret_code_t err = rtc_read(RTC_REG_CTRL2, &ctrl2, 1);
    if (err != NRF_SUCCESS)
        return err;

    ctrl2 &= ~RTC_CTRL2_AIE; // Clear Alarm Interrupt Enable bit
    return rtc_write(RTC_REG_CTRL2, &ctrl2, 1);
}