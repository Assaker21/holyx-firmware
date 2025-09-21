#ifndef IMAGE_RECEIVER_H
#define IMAGE_RECEIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*frame_callback_t)(const uint8_t *image_data, uint16_t image_len, const char *timestamp, const char *next_timestamp, const char *schedule);

/**
 * Call once at boot – pass the same secret you use on the phone.
 */
void image_receiver_init(const char *auth_key);

/**
 * Feed every 18-byte ATT write that arrives on the **image** characteristic.
 * Exactly the same pointer/length pair you already print in the handler.
 */
void image_receiver_handle_chunk(const uint8_t *data, uint16_t len, frame_callback_t callback);

#endif /* IMAGE_RECEIVER_H */