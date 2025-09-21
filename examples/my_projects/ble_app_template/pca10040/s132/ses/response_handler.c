#include "response_handler.h"
#include "nrf_pwr_mgmt.h"
#include "sha256.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define START_TAG "STARTSEND"
#define END_TAG "ENDSEND"
#define DELIM "____"
#define NONCE_LEN 16     /* 8 random bytes, hex-encoded  -> 16 chars  */
#define HASH_LEN 64      /* SHA-256 hex string                         */
#define MAX_PAYLOAD 3700 /* plenty for one whole transfer (~700 B)     */
#define MAX_NONCES 8     /* rolling log to spot replays                */

static char frame_buf[MAX_PAYLOAD];
static size_t frame_len = 0;
static bool collecting = false;

static char auth_key_local[80] = {0}; /* user-supplied               */
static char nonce_log[MAX_NONCES][NONCE_LEN + 1];
static uint8_t nonce_idx = 0;

static int8_t b64lut[256];
static void b64lut_init(void)
{
    memset(b64lut, -1, sizeof(b64lut));
    for (uint8_t c = 'A'; c <= 'Z'; ++c)
        b64lut[c] = c - 'A';
    for (uint8_t c = 'a'; c <= 'z'; ++c)
        b64lut[c] = 26 + c - 'a';
    for (uint8_t c = '0'; c <= '9'; ++c)
        b64lut[c] = 52 + c - '0';
    b64lut[(uint8_t)'+'] = 62;
    b64lut[(uint8_t)'/'] = 63;
    b64lut[(uint8_t)'='] = 0;
}

static size_t base64_decode(const char *in, size_t len, uint8_t *out)
{
    size_t out_len = 0;
    uint32_t buf = 0;
    uint8_t bits = 0;

    for (size_t i = 0; i < len; ++i)
    {
        int8_t v = b64lut[(uint8_t)in[i]];
        if (v < 0)
            continue; /* ignore odd chars / padding */

        buf = (buf << 6) | (uint32_t)v;
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out[out_len++] = (uint8_t)((buf >> bits) & 0xFF);
        }
    }
    return out_len;
}
/* -------------------------------------------------------------------------- */

void image_receiver_init(const char *auth_key)
{
    strncpy(auth_key_local, auth_key, sizeof(auth_key_local) - 1);
    b64lut_init();
}

void turn_off()
{
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF);
}

static void process_complete_frame(frame_callback_t callback)
{

    printf("Started processing complete frame\n\r");
    // [nonce]____[hash][timestamp][imageBase64]
    char *p = frame_buf;

    // 1. Nonce
    char nonce[NONCE_LEN + 1] = {0};
    memcpy(nonce, p, NONCE_LEN);
    p += NONCE_LEN;

    for (uint8_t i = 0; i < MAX_NONCES; ++i)
    {
        if (nonce_log[i][0] && /* slot in use?              */
            strncmp(nonce, nonce_log[i], NONCE_LEN) == 0)
        {
            puts("Nonce replay – discarding frame");
            return;
        }
    }

    // 2. delim “____”
    if (strncmp(p, DELIM, 4) != 0)
    {
        puts("Format error");
        return;
    }
    p += 4;

    // 3. hash
    char sig[HASH_LEN + 1] = {0};
    memcpy(sig, p, HASH_LEN);
    p += HASH_LEN;

    // 4. timestamp – decimal digits until non-digit
    printf("P: %d\n\r", p);
    char timestamp[12] = {0};
    uint8_t tlen = 0;
    while (isdigit((unsigned char)*p) && tlen < 10)
        timestamp[tlen++] = *p++;
    timestamp[tlen] = '\0';

    printf("P: %d\n\r", p);
    while (*p && !isdigit((unsigned char)*p))
        p++; // advance until next digit
    printf("P: %d\n\r", p);

    char next_timestamp[12] = {0};
    uint8_t next_tlen = 0;
    while (isdigit((unsigned char)*p) && next_tlen < 10)
        next_timestamp[next_tlen++] = *p++;
    next_timestamp[next_tlen] = '\0';


    char schedule[100] = {0};
    next_tlen = 0;
    while (isdigit((unsigned char)*p) && next_tlen < sizeof(schedule))
        schedule[next_tlen++] = *p++;
    schedule[next_tlen] = '\0';

    printf("Schedule: %s\n\r", schedule);

    

    printf("P: %d\n\r", p);

    // 5. imageBase64 (rest of buffer)
    const char *image_b64 = p;
    size_t image_b64_len = frame_len - (size_t)(p - frame_buf);

    printf("%s____%s____%s____", nonce, timestamp, next_timestamp);
    printf("%s", schedule);

    // 6. Verify signature
    char msg_for_hash[MAX_PAYLOAD];

    size_t msg_len = snprintf(msg_for_hash, sizeof(msg_for_hash),
        "%s____%s____%s____%s____%.*s%s",
        nonce, timestamp, next_timestamp, schedule, (int)image_b64_len, image_b64, auth_key_local);

    printf("Message length: %d\n\r", msg_len);

    char calc_sig[65];
    if (!sha256_hex(msg_for_hash, msg_len, calc_sig))
    // if (!sha256_hex("test messagetest messagetest messagetest messagetest messagetest messagetest messagetest messagetest messagetest message", 120, calc_sig))
    {
        puts("SHA-256 fail");
        return;
    }

    if (strncmp(sig, calc_sig, HASH_LEN) != 0)
    {

        puts("Signature mismatch – discarding frame");
        return;
    }

    // 7. Decode image
    uint8_t image_bytes[3200]; /* adjust if you need more   */
    size_t img_len = base64_decode(image_b64, image_b64_len, image_bytes);

    // 8. Success output
    printf("Received data: \n\r");
    printf("Nonce: %s\n\r", nonce);
    printf("Timestamp = %s  (sig OK)\r\n", timestamp);
    printf("Next Timestamp = %s  (sig OK)\r\n", next_timestamp);
    printf("Schedule: %s\n\r", schedule);
    printf("Image size = %u bytes\r\n", (unsigned)img_len);

    if (callback != NULL)
    {
    printf("Complete frame can be processed!\r\n");
        callback(image_bytes, img_len, timestamp, next_timestamp, schedule);
    }
    else {
       printf("Callback is null!!\r\n");
    
    }

    // 9. Store nonce so you can ignore replays
    strncpy(nonce_log[nonce_idx], nonce, NONCE_LEN + 1);
    nonce_idx = (nonce_idx + 1) % MAX_NONCES;

    /* ?  At this point `image_bytes[0..img_len-1]` holds the 1-D bitmap   */
    /*    Do whatever you need – render, store to flash, etc.              */
}

void image_receiver_handle_chunk(const uint8_t *data, uint16_t len, frame_callback_t callback)
{
    static int counter = 0;
    for (uint16_t i = 0; i < len; ++i)
    {
        // 0) Waiting for “STARTSEND”
        if (!collecting)
        {
            static uint8_t match = 0;
            if (data[i] == START_TAG[match])
            {
                match++;
                if (START_TAG[match] == '\0')
                {
                    collecting = true;
                    frame_len = 0;
                    match = 0;
                }
            }
            else
                match = 0;
            continue;
        }

        // 1) Collect until we see "ENDSEND"
        if (frame_len < sizeof(frame_buf) - 1)

        {
            // printf("Adding chunks\n\r");
            frame_buf[frame_len++] = (char)data[i];
        }

        size_t tag_len = strlen(END_TAG);
        if (frame_len >= tag_len &&
            memcmp(&frame_buf[frame_len - tag_len], END_TAG, tag_len) == 0)
        {

            frame_len -= tag_len; /* strip END_TAG                     */
            frame_buf[frame_len] = '\0';
            collecting = false;
            process_complete_frame(callback);
        }
    }
}