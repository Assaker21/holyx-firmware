#ifndef EPD_DISPLAY_H
#define EPD_DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_util_platform.h"
#include "boards.h"
#include "nrf_delay.h"
#include "nrf_drv_spi.h"
#include "pin_definitions.h"

#ifdef __cplusplus
extern "C"
{
#endif

// 98x184 pixels = 18032 bits = 2254 bytes
#define BITMAP_EXPECTED_SIZE 3300

// ------------------------ Display Properties ------------------------
#define EPD_WIDTH 96
#define EPD_HEIGHT 184
#define EPD_BUFFER_SIZE 3300

#define EPD_BLACK 0
#define EPD_WHITE 1

    // ------------------------ External Data ------------------------
    /**
     * font_5x7:
     *  - 5 bytes per character
     *  - Index 0 is SPACE
     *  - Indices 1..26 map to 'A'..'Z'
     */
    extern const uint8_t font_5x7[][5];

    /**
     * The global e-paper framebuffer: 1 bit per pixel (black/white).
     */
    extern uint8_t g_epd_buffer[];

    // ------------------------ Function Prototypes ------------------------
    /**
     * @brief Initializes the SPI peripheral and configures GPIO for the e-paper display.
     */
    void epd_spi_init(void);

    /**
     * @brief Sends a single command byte to the display.
     * @param cmd Command byte
     */
    void epd_send_command(uint8_t cmd);

    /**
     * @brief Sends a single data byte to the display.
     * @param data Data byte
     */
    void epd_send_data(uint8_t data);

    /**
     * @brief Sends a buffer (array of bytes) as data to the display.
     * @param buf Pointer to buffer
     * @param len Number of bytes
     */
    void epd_send_buffer(const uint8_t *buf, size_t len);

    /**
     * @brief Blocks until the e-paper BUSY pin indicates ready.
     */
    void epd_wait_busy(void);

    /**
     * @brief Hardware reset of the display (toggle RESET pin).
     */
    void epd_reset(void);

    /**
     * @brief Initializes (resets) the display and issues a SWRESET command.
     */
    void epd_init(void);

    /**
     * @brief Puts the e-paper display into deep sleep mode.
     */
    void epd_deep_sleep(void);

    /**
     * @brief Sets the pixel at coordinates (x,y) to either black or white.
     * @param x     X coordinate [0..EPD_WIDTH-1]
     * @param y     Y coordinate [0..EPD_HEIGHT-1]
     * @param color EPD_BLACK or EPD_WHITE
     */
    void epd_draw_pixel(int x, int y, uint8_t color);

    /**
     * @brief Fills the entire buffer with either 0x00 or 0xFF (for black or white).
     * @param color Pass 0x00 => all bits cleared, 0xFF => all bits set
     */
    void epd_full_color(uint8_t color);

    /**
     * @brief Updates (refreshes) the entire display using the contents of g_epd_buffer.
     */
    void epd_update_full(void);

    /**
     * @brief Draws a single character (A-Z or space) at (x,y) in black.
     * @param x X coordinate
     * @param y Y coordinate
     * @param c Character to draw (A-Z or space)
     */
    void epd_draw_char(int x, int y, char c);

    /**
     * @brief Draws a null-terminated string in black, starting at (x,y).
     * @param x   Starting X coordinate
     * @param y   Starting Y coordinate
     * @param str Pointer to string
     */
    void epd_draw_text(int x, int y, const char *str);

    bool epd_load_base64_bitmap(const char *base64_str);

    static size_t base64_decode(const char *input, uint8_t *output, size_t max_output_len);

    static int base64_char_value(char c);

    bool epd_load_raw_bitmap(const char *char_buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif // EPD_DISPLAY_H