#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_delay.h"  
#include "rtc.h"

int main(void)
{
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();

    twi_init();

    rtc_datetime_t now = {
        .sec   = 0,
        .min   = 0,
        .hour  = 19,
        .wday  = 2,
        .mday  = 27,
        .month = 5,
        .year  = 25
    };
    APP_ERROR_CHECK(rtc_set_datetime(&now));
    print_datetime("Set:", &now);

    while (true)
    {
        APP_ERROR_CHECK(rtc_get_datetime(&now));
        print_datetime("Now:", &now);
        NRF_LOG_FLUSH();
        nrf_delay_ms(3000);
    }
}
