#ifndef POWER_MANAGER_H
#define POWER_MANAER_H

/* turn_off.c ---------------------------------------------------- */
#include "nrf.h"                /* Core nRF register definitions   */
#include "nrf_power.h"          /* POWER peripheral (bare-metal)   */
#include "nrf_pwr_mgmt.h"       /* If you already use the module   */
#include "nrf_gpio.h"
#include "app_error.h"
#include "nrf_delay.h"

#define WAKE_PIN   15
#define WAKE_POLARITY NRF_GPIO_PIN_SENSE_HIGH
#define WAKE_PULL     NRF_GPIO_PIN_PULLDOWN 


void turn_off_reset_required(void);

#endif
