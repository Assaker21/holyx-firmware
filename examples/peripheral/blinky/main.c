#include "nrf_delay.h"
#include "nrf_gpio.h"
#include <stdbool.h>
#include <stdint.h>

int main(void)
{
    nrf_gpio_cfg_output(22);

    while (1)
    {
        nrf_gpio_pin_set(22);
        nrf_delay_ms(500);
        nrf_gpio_pin_clear(22);
        nrf_delay_ms(500);
    }
}