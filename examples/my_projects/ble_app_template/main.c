#include "ble_api.h"
#include "eeprom.h"
#include "epd.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "power_manager.h"
#include "rtc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int turn_off_ready = 0;

void turn_off_using_rtc()
{
    clear_alarm_flag();
}

void print_image_binary(const uint8_t *image_data)
{
    for (int y = 0; y < 184; y++)
    {
        for (int x_byte = 0; x_byte < 96 / 8; x_byte++)
        {
            uint8_t byte = image_data[y * (96 / 8) + x_byte];
            for (int bit = 7; bit >= 0; bit--)
            {
                printf("%c", (byte & (1 << bit)) ? '1' : '0');
            }
        }
        printf("\n");
    }
}

int parse_unix_timestamp_string(const char *timestamp_str, rtc_datetime_t *dt)
{
    if (!timestamp_str || strlen(timestamp_str) != 10 || !dt)
        return -1;

    time_t timestamp = (time_t)strtoul(timestamp_str, NULL, 10);
    timestamp += 30;
    struct tm *tm_utc = gmtime(&timestamp);

    if (!tm_utc)
        return -2;

    dt->sec = tm_utc->tm_sec;
    dt->min = tm_utc->tm_min;
    dt->hour = tm_utc->tm_hour;
    dt->wday = tm_utc->tm_wday;
    dt->mday = tm_utc->tm_mday;
    dt->month = tm_utc->tm_mon + 1;
    dt->year = (tm_utc->tm_year >= 100) ? (tm_utc->tm_year - 100) : 0;

    return 0;
}

void handle_frame(const uint8_t *image_data, size_t image_len, const char *timestamp, const char *next_timestamp, const char *schedule)
{
    nrf_delay_ms(150);
    printf("Callback Timestamp: %s\n\r", timestamp);
    rtc_datetime_t dt;
    rtc_datetime_t next_dt;

    ret_code_t err = eeprom_write(schedule, 100);
    APP_ERROR_CHECK(err);

    if (parse_unix_timestamp_string(timestamp, &dt) == 0 && parse_unix_timestamp_string(next_timestamp, &next_dt) == 0)
    {
        printf("Parsed Date: %02u-%02u-%02u %02u:%02u:%02u (WDay: %u)\n\r",
            dt.year, dt.month, dt.mday,
            dt.hour, dt.min, dt.sec,
            dt.wday);

        printf("Setting RTC to: %02u-%02u-%02u %02u:%02u:%02u\n\r",
            dt.year, dt.month, dt.mday,
            dt.hour, dt.min, dt.sec);

        ret_code_t err = rtc_set_datetime(&dt);
        if (err != NRF_SUCCESS)
        {
            printf("Failed to set RTC time: %d\n\r", err);
            return;
        }

        printf("Set RTC time successfully\n\r");

        // Optional: Display image on EPD
        if (image_data && image_len > 0)
        {
            print_image_binary(image_data);
            epd_spi_init();
            epd_init();
            epd_full_color(0xFF);
            memcpy(g_epd_buffer, image_data, image_len);
            epd_update_full();

            printf("Updating display with %zu bytes of image data\n\r", image_len);
            // epd_display_image(image_data, image_len);
        }

        // Set alarm 1 minute from the set time
        // uint8_t alarm_min = (dt.min + 2) % 60;
        // uint8_t alarm_hour = dt.hour;

        //// Handle hour rollover if minute wrapped around
        // if (alarm_min == 0)
        //{
        //     alarm_hour = (dt.hour + 1) % 24;
        // }

        // printf("Setting alarm for %02u:%02u\n\r", alarm_hour, alarm_min);

        err = rtc_set_alarm_time(next_dt.hour, next_dt.min);
        if (err == NRF_SUCCESS)
        {
            printf("Alarm set successfully!\n\r");
        }
        else
        {
            printf("Failed to set alarm: %d\n\r", err);
            return;
        }

        nrf_delay_ms(150);

        // Verify the alarm was set correctly
        rtc_alarm_t read_back;
        if (rtc_get_alarm(&read_back) == NRF_SUCCESS)
        {
            printf("Alarm verification: %02u:%02u\n\r", read_back.hour, read_back.min);
        }

        // clear_alarm_flag();
        // nrf_delay_ms(5000);
        // turn_off_using_rtc();
        //  turn_off_reset_required();

        nrf_delay_ms(100);
        nrf_gpio_pin_clear(12);
    }
    else
    {
        printf("Invalid timestamp input.\n\r");
    }
}

int main(void)
{
    nrf_gpio_cfg_output(12);
    nrf_gpio_pin_set(12);
    // nrf_gpio_pin_clear(21);

    nrf_delay_ms(200);
    printf("Initializing...\n\r");

    // nrf_gpio_cfg_input(15, NRF_GPIO_PIN_PULLDOWN);
    //  nrf_gpio_cfg_output(21);
    nrf_delay_ms(200);

    // nrf_delay_ms(1000);

    printf("Setup alarm flag");

    // nrf_gpio_pin_set(21);

    // if (0 && nrf_gpio_pin_read(15))
    //{
    //     //  Initialize RTC first
    //     printf("STARTED: RTC initializing\n\r");
    //     rtc_init(1);
    //     printf("RTC initialized\n\r");
    //     nrf_delay_ms(200);
    //     rtc_set_alarm_flag();
    //     // nrf_gpio_pin_set(21);
    // }
    // else
    //{
    printf("STARTED: RTC initializing\n\r");
    rtc_init(1);
    printf("RTC initialized\n\r");
    nrf_delay_ms(200);
    // clear_alarm_flag();
    printf("Cleared alarm flag\n\r");
    //}

    ret_code_t err = eeprom_init();
    APP_ERROR_CHECK(err);
    printf("EEPROM initialized\n\r");

    // Initialize BLE
    ble_api_init(handle_frame);
    printf("BLE initialized\n\r");

    // nrf_gpio_pin_clear(12);

    // NRF_GPIO->LATCH = 1UL << WAKE_PIN;

    // nrf_gpio_cfg_sense_input(WAKE_PIN,
    //     WAKE_PULL,
    //     WAKE_POLARITY);

    printf("System ready, waiting for BLE data...\n\r");

    while (1)
    {
        //__WFE();
    }
}