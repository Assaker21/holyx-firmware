#ifndef BLE_API_H
#define BLE_API_H

#include <stdbool.h>
#include <stdint.h>
#include "response_handler.h"

#define DEVICE_ID "K652"

#ifdef __cplusplus
extern "C"
{
#endif

    void ble_api_init(frame_callback_t callback);

    void ble_api_process(void);

#ifdef __cplusplus
}
#endif

#endif /* BLE_API_H */