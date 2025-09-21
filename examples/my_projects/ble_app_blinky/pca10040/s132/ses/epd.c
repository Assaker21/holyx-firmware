#include "epd.h"

// ------------------------ Global Data ------------------------
// Remove 'static' so it matches extern in epd.h
const uint8_t font_5x7[][5] = {
    [0] = {0x00, 0x00, 0x00, 0x00, 0x00},  // SPACE
    [1] = {0x7C, 0x12, 0x11, 0x12, 0x7C},  // A
    [2] = {0x7F, 0x49, 0x49, 0x49, 0x36},  // B
    [3] = {0x3E, 0x41, 0x41, 0x41, 0x22},  // C
    [4] = {0x7F, 0x41, 0x41, 0x22, 0x1C},  // D
    [5] = {0x7F, 0x49, 0x49, 0x49, 0x41},  // E
    [6] = {0x7F, 0x09, 0x09, 0x09, 0x01},  // F
    [7] = {0x3E, 0x41, 0x49, 0x49, 0x7A},  // G
    [8] = {0x7F, 0x08, 0x08, 0x08, 0x7F},  // H
    [9] = {0x41, 0x41, 0x7F, 0x41, 0x41},  // I
    [10] = {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    [11] = {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    [12] = {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    [13] = {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    [14] = {0x7F, 0x06, 0x18, 0x30, 0x7F}, // N
    [15] = {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    [16] = {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    [17] = {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    [18] = {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    [19] = {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    [20] = {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    [21] = {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    [22] = {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    [23] = {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    [24] = {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    [25] = {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    [26] = {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
};

// Remove 'static' so it matches extern in epd.h
uint8_t g_epd_buffer[EPD_BUFFER_SIZE];

// SPI instance
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);

// ------------------------ Inline Utilities (private) ------------------------
static inline void digital_write(uint32_t pin, uint8_t value)
{
    nrf_gpio_pin_write(pin, value ? 1 : 0);
}
static inline uint8_t digital_read(uint32_t pin)
{
    return nrf_gpio_pin_read(pin);
}
static inline void epd_select(void)   { digital_write(EPD_CS_PIN, 0); }
static inline void epd_unselect(void) { digital_write(EPD_CS_PIN, 1); }
static inline void epd_cmd_mode(void) { digital_write(EPD_DC_PIN, 0); }
static inline void epd_data_mode(void){ digital_write(EPD_DC_PIN, 1); }

// ------------------------ Public Functions ------------------------
void epd_spi_init(void)
{
    // Configure e-paper pins
    nrf_gpio_cfg_output(EPD_CS_PIN);
    nrf_gpio_cfg_output(EPD_DC_PIN);
    nrf_gpio_cfg_output(EPD_RESET_PIN);
    nrf_gpio_cfg_input(EPD_BUSY_PIN, NRF_GPIO_PIN_NOPULL);

    digital_write(EPD_CS_PIN, 1);
    digital_write(EPD_DC_PIN, 1);
    digital_write(EPD_RESET_PIN, 1);

    // Set up SPI
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.sck_pin      = EPD_SPI_SCK_PIN;
    spi_config.mosi_pin     = EPD_SPI_MOSI_PIN;
    spi_config.miso_pin     = EPD_SPI_MISO_PIN;
    spi_config.ss_pin       = NRF_DRV_SPI_PIN_NOT_USED;
    spi_config.frequency    = NRF_DRV_SPI_FREQ_4M;
    spi_config.mode         = NRF_DRV_SPI_MODE_0;
    spi_config.bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST;

    nrf_drv_spi_init(&spi, &spi_config, NULL, NULL);
}

void epd_send_command(uint8_t cmd)
{
    epd_select();
    epd_cmd_mode();
    nrf_drv_spi_transfer(&spi, &cmd, 1, NULL, 0);
    epd_unselect();
}

void epd_send_data(uint8_t data)
{
    epd_select();
    epd_data_mode();
    nrf_drv_spi_transfer(&spi, &data, 1, NULL, 0);
    epd_unselect();
}

void epd_send_buffer(const uint8_t *buf, size_t len)
{
    epd_select();
    epd_data_mode();
    nrf_drv_spi_transfer(&spi, buf, len, NULL, 0);
    epd_unselect();
}

void epd_wait_busy(void)
{
    // Busy pin is active LOW, so wait while pin = 1
    while (digital_read(EPD_BUSY_PIN))
    {
        nrf_delay_ms(5);
    }
}

void epd_reset(void)
{
    digital_write(EPD_RESET_PIN, 0);
    nrf_delay_ms(2);
    digital_write(EPD_RESET_PIN, 1);
    nrf_delay_ms(2);
}

void epd_init(void)
{
    // Ensure pins are configured
    epd_spi_init();

    // Hardware reset
    epd_reset();
    epd_wait_busy();

    // Software reset
    epd_send_command(0x12);
    epd_wait_busy();
}

void epd_deep_sleep(void)
{
    epd_send_command(0x10);
    epd_send_data(0x01); // Enter deep sleep
}

void epd_draw_pixel(int x, int y, uint8_t color)
{
    if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT)
        return;

    uint16_t byte_idx = (y * (EPD_WIDTH / 8)) + (x / 8);
    uint8_t bit       = 7 - (x % 8);

    if (color == EPD_BLACK)
        g_epd_buffer[byte_idx] &= ~(1 << bit);
    else
        g_epd_buffer[byte_idx] |= (1 << bit);
}

void epd_full_color(uint8_t color)
{
    // If color == 0x00 -> all black; if color == 0xFF -> all white
    memset(g_epd_buffer, color, EPD_BUFFER_SIZE);
}

void epd_update_full(void)
{
    // Set RAM X range: 0..(EPD_WIDTH/8 - 1)
    epd_send_command(0x44);
    epd_send_data(0x00);
    epd_send_data(0x0B); // 0x0A means 10 in decimal => 11 bytes wide

    // Set RAM Y range: 0..183
    epd_send_command(0x45);
    epd_send_data(0x00);
    epd_send_data(0x00);
    epd_send_data(0xB7);
    epd_send_data(0x00);

    // Write the buffer to the display RAM
    for (int y = 0; y < EPD_HEIGHT; y++)
    {
        for (int x_byte = 0; x_byte < (EPD_WIDTH / 8); x_byte++)
        {
            // Set X address pointer
            epd_send_command(0x4E);
            epd_send_data(x_byte);

            // Set Y address pointer
            epd_send_command(0x4F);
            epd_send_data((uint8_t)(y & 0xFF));
            epd_send_data((uint8_t)((y >> 8) & 0xFF));

            epd_wait_busy();

            // Write 1 byte to RAM
            epd_send_command(0x24); // Write RAM
            uint8_t data = g_epd_buffer[y * (EPD_WIDTH / 8) + x_byte];
            epd_send_data(data);
        }
    }

    // Trigger a display refresh
    epd_send_command(0x20);
    epd_wait_busy();
}

void epd_draw_char(int x, int y, char c)
{
    if (c == ' ') {
        // Space is a blank area, do nothing
        return;
    }
    if (c >= 'A' && c <= 'Z')
    {
        const uint8_t *glyph = font_5x7[(c - 'A') + 1];
        for (int col = 0; col < 5; col++)
        {
            for (int row = 0; row < 7; row++)
            {
                if (glyph[col] & (1 << row))
                {
                    epd_draw_pixel(x + col, y + row, EPD_BLACK);
                }
            }
        }
    }
}

void epd_draw_text(int x, int y, const char *str)
{
    while (*str)
    {
        epd_draw_char(x, y, *str);
        x += 6; // 5-pixel glyph width + 1 spacing
        str++;
    }
}

static int base64_char_value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;  // Invalid base64 char
}

// Decode base64 into a byte buffer
static size_t base64_decode(const char *input, uint8_t *output, size_t max_output_len)
{
    size_t input_len = strlen(input);
    size_t out_index = 0;
    uint32_t buffer = 0;
    int bits_collected = 0;

    for (size_t i = 0; i < input_len; i++)
    {
        char c = input[i];
        if (c == '=')
            break;  // Padding character, stop decoding

        int val = base64_char_value(c);
        if (val < 0)
            {
            printf("INVALID CHARACTER");
            continue; 
            
            } // Skip invalid characters

            printf("%c=%d", c, val);

        buffer = (buffer << 6) | val;
        bits_collected += 6;

        if (bits_collected >= 8)
        {
            bits_collected -= 8;
            if (out_index < max_output_len)
            {
                output[out_index++] = (uint8_t)((buffer >> bits_collected) & 0xFF);
            }
            else
            {
                // Output buffer overflow
                return 0;
            }
        }
    }

    return out_index;
}

// Main function to decode base64 image data and update the EPD
bool epd_load_base64_bitmap(const char *base64_str)
{
    uint8_t decoded_data[BITMAP_EXPECTED_SIZE];
    size_t decoded_len = base64_decode(base64_str, decoded_data, sizeof(decoded_data));

    if (decoded_len != BITMAP_EXPECTED_SIZE)
    {
        // Invalid size or failed decoding
        return false;
    }

    // Load into display buffer
    memcpy(g_epd_buffer, decoded_data, BITMAP_EXPECTED_SIZE);
    epd_update_full();
    return true;
}

bool epd_load_raw_bitmap(const char *char_buf, size_t buf_len)
{
    if (buf_len != BITMAP_EXPECTED_SIZE)
    {
        // Incorrect buffer size
        return false;
    }

    // Convert each char to byte and copy into g_epd_buffer
    for (size_t i = 0; i < buf_len; i++)
    {
        g_epd_buffer[i] = (uint8_t)(unsigned char)char_buf[i];  // Direct byte copy
    }

    epd_update_full();
    return true;
}
