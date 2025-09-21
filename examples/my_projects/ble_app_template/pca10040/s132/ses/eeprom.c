#include "eeprom.h"
#include "fds.h"
#include "nrf_log.h"

static bool fds_ready = false;

static void fds_evt_handler(fds_evt_t const * p_evt)
{
    if (p_evt->id == FDS_EVT_INIT) {
        fds_ready = (p_evt->result == NRF_SUCCESS);
    }
}

ret_code_t eeprom_init(void)
{
    ret_code_t err;

    err = fds_register(fds_evt_handler);
    if (err != NRF_SUCCESS) return err;

    err = fds_init();
    if (err != NRF_SUCCESS) return err;

    while (!fds_ready) {
        // Wait until FDS is initialized
        __WFE();
    }

    return NRF_SUCCESS;
}

ret_code_t eeprom_write(const char* data, uint16_t length)
{
    fds_record_t record = {
        .file_id = EEPROM_FILE_ID,
        .key = EEPROM_REC_KEY,
        .data.p_data = data,
        .data.length_words = (length + 3) / 4
    };

    // Delete existing
    fds_find_token_t tok = {0};
    fds_record_desc_t desc;
    if (fds_record_find(EEPROM_FILE_ID, EEPROM_REC_KEY, &desc, &tok) == NRF_SUCCESS) {
        fds_record_delete(&desc);
        fds_gc();  // Optional: Run GC to reclaim space
    }

    return fds_record_write(&desc, &record);
}

ret_code_t eeprom_read(char* buffer, uint16_t* length)
{
    fds_find_token_t tok = {0};
    fds_record_desc_t desc;
    fds_flash_record_t flash_rec;

    if (fds_record_find(EEPROM_FILE_ID, EEPROM_REC_KEY, &desc, &tok) != NRF_SUCCESS) {
        return NRF_ERROR_NOT_FOUND;
    }

    ret_code_t err = fds_record_open(&desc, &flash_rec);
    if (err != NRF_SUCCESS) return err;

    uint16_t bytes = flash_rec.p_header->length_words * 4;
    if (bytes > *length) bytes = *length;

    memcpy(buffer, flash_rec.p_data, bytes);
    *length = bytes;

    return fds_record_close(&desc);
}

ret_code_t eeprom_clear(void)
{
    fds_find_token_t tok = {0};
    fds_record_desc_t desc;
    if (fds_record_find(EEPROM_FILE_ID, EEPROM_REC_KEY, &desc, &tok) == NRF_SUCCESS) {
        return fds_record_delete(&desc);
    }
    return NRF_ERROR_NOT_FOUND;
}
