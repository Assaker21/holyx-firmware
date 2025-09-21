#include "power_manager.h"
#include "nrf_gpio.h"
#ifdef SOFTDEVICE_PRESENT
#include "ble_gap.h"          /* or ble_hci.h */
#endif

static void configure_wake_pin(void)
{
    /* Clear previous latch (optional but tidy) */
    NRF_GPIO->LATCH = 1UL << WAKE_PIN;

    nrf_gpio_cfg_sense_input(WAKE_PIN,
                             WAKE_PULL,
                             WAKE_POLARITY);
}

void turn_off_reset_required(void)
{
    printf("Turning off!\n");

    /* 1.  Disable all wake sources, *then* enable the one we want */
    for (uint32_t pin = 0; pin < 32; pin++)
        NRF_GPIO->PIN_CNF[pin] &= ~GPIO_PIN_CNF_SENSE_Msk;  /* SENSE = NONE */

    configure_wake_pin();               /* << our only escape hatch */

#if defined(SOFTDEVICE_PRESENT)         /* -------- with SoftDevice */
    printf("System OFF via SoftDevice\n");
    APP_ERROR_CHECK(sd_power_system_off());  /* IRQs must stay ON   */
    /* never returns */
#else                                    /* -------- bare-metal     */
    __disable_irq();                     /* safe: no SVC needed     */
    printf("System OFF bare-metal\n");
    NRF_POWER->SYSTEMOFF = 1;
    __WFE();                             /* never returns           */
#endif
}