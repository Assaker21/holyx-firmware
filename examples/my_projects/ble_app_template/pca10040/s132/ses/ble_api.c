#include "ble_api.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "app_error.h"
#include "app_timer.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_srv_common.h"
#include "eeprom.h"
#include "nrf.h"
#include "nrf_ble_gatt.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "rtc.h"

#include "response_handler.h"

//#define DEVICE_NAME "HOLYX-223542"
#define APP_ADV_INTERVAL 64  /* 40 ms                */
#define APP_ADV_DURATION 120 /* keep advertising     */
#define APP_BLE_CONN_CFG_TAG 1
#define APP_BLE_OBSERVER_PRIO 3

#define SERVICE_UUID_BASE                                                                              \
    {                                                                                                  \
        0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x34, 0x12, 0x00, 0x00 \
    }
#define SERVICE_UUID_SHORT 0x1234
#define CHAR_UUID_SHORT 0x5678
#define CHAR_UUID_TIMESTAMP 0x5679
#define CHAR_UUID_IMAGE 0x6679

static char m_device_name[13];
static uint8_t m_uuid_type;
static uint16_t m_service_handle;
static ble_gatts_char_handles_t m_char_handles_auth;
static ble_gatts_char_handles_t m_char_handles_ts;
static ble_gatts_char_handles_t m_char_handles_img;

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

static frame_callback_t receive_callback;

NRF_BLE_GATT_DEF(m_gatt);
BLE_ADVERTISING_DEF(m_adv);

void hash6(const char *input, char out[7])
{
    const uint64_t FNV_OFFSET = 1469598103934665603ULL;
    const uint64_t FNV_PRIME = 1099511628211ULL;

    // 64-bit FNV-1a over the input bytes
    uint64_t h = FNV_OFFSET;
    const unsigned char *p = (const unsigned char *)input;
    while (*p)
    {
        h ^= (uint64_t)(*p++);
        h *= FNV_PRIME; // wraps modulo 2^64 for uint64_t
    }

    // base-36 fixed-width (6 chars)
    uint64_t base = 1;
    for (int i = 0; i < 6; i++)
        base *= 36;

    uint64_t v = h % base;
    for (int i = 5; i >= 0; i--)
    {
        uint64_t digit = v % 36;
        v /= 36;
        out[i] = (digit < 10) ? ('0' + digit) : ('A' + (digit - 10));
    }
    out[6] = '\0';
}

static void logs_init(void)
{
    APP_ERROR_CHECK(NRF_LOG_INIT(NULL));
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void power_init(void)
{
    APP_ERROR_CHECK(nrf_pwr_mgmt_init());
}

static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    switch (ble_adv_evt)
    {
    case BLE_ADV_EVT_IDLE:
    {
        printf("Advertising timed out (no connection). Going to system OFF.");
        // NRF_LOG_INFO("Advertising timed out (no connection). Going to system OFF.");
        //  Go to System OFF (will wake on reset pin or configured wake sources)

        char *schedule;
        eeprom_read(schedule, 100);
        nrf_delay_ms(100);

        rtc_datetime_t time_now, next_time;
        rtc_get_datetime(&time_now);

        nrf_delay_ms(100);

        int found = 0;
        for (int i = 0; i + 3 < 100; i += 4)
        {
            if (schedule[i] == ' ')
                break;
            int hh = (schedule[i] - '0') * 10 + (schedule[i + 1] - '0');
            int mm = (schedule[i + 2] - '0') * 10 + (schedule[i + 3] - '0');

            if (hh > time_now.hour || (hh == time_now.hour && mm >= time_now.min))

                next_time.hour = (uint8_t)hh;
            next_time.min = (uint8_t)mm;
            found = 1;
            return;
        }

        if (!found && schedule[0] != ' ')
        {
            int hh = (schedule[0] - '0') * 10 + (schedule[0 + 1] - '0');
            int mm = (schedule[0 + 2] - '0') * 10 + (schedule[0 + 3] - '0');

            next_time.hour = (uint8_t)hh;
            next_time.min = (uint8_t)mm;

            found = 1;
        }

        if (found)
        {
            rtc_set_alarm_time(next_time.hour, next_time.min);
        }

        nrf_delay_ms(100);
        nrf_gpio_pin_clear(12);
        break;
        
    }
    }
}

static void gap_params_init(void)
{
    ble_gap_conn_params_t cp = {0};
    ble_gap_conn_sec_mode_t sec_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    sd_ble_gap_device_name_set(&sec_mode,
        (const uint8_t *)m_device_name,
        strlen(m_device_name));

    cp.min_conn_interval = MSEC_TO_UNITS(7.5, UNIT_1_25_MS);
    cp.max_conn_interval = MSEC_TO_UNITS(7.5, UNIT_1_25_MS);
    cp.slave_latency = 0;
    cp.conn_sup_timeout = MSEC_TO_UNITS(4000, UNIT_10_MS);

    sd_ble_gap_ppcp_set(&cp);
}

static void gatt_init(void)
{
    APP_ERROR_CHECK(nrf_ble_gatt_init(&m_gatt, NULL));
}

static void service_init(void)
{
    ret_code_t err;
    ble_uuid_t svc_uuid;
    ble_uuid128_t base_uuid = {SERVICE_UUID_BASE};

    err = sd_ble_uuid_vs_add(&base_uuid, &m_uuid_type);
    APP_ERROR_CHECK(err);

    svc_uuid.type = m_uuid_type;
    svc_uuid.uuid = SERVICE_UUID_SHORT;

    err = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
        &svc_uuid,
        &m_service_handle);
    APP_ERROR_CHECK(err);

#define ADD_CHAR(_short_uuid, _handles)                                    \
    do                                                                     \
    {                                                                      \
        ble_gatts_char_md_t char_md = {0};                                 \
        ble_gatts_attr_md_t attr_md = {0};                                 \
        ble_gatts_attr_t attr = {0};                                       \
        uint8_t init_val = 0;                                              \
                                                                           \
        char_md.char_props.write = 1;                                      \
        char_md.char_props.write_wo_resp = 1;                              \
                                                                           \
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);                \
        BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);               \
        attr_md.vloc = BLE_GATTS_VLOC_STACK;                               \
        attr_md.vlen = 1;                                                  \
                                                                           \
        ble_uuid_t char_uuid = {.type = m_uuid_type, .uuid = _short_uuid}; \
                                                                           \
        attr.p_uuid = &char_uuid;                                          \
        attr.p_attr_md = &attr_md;                                         \
        attr.init_len = sizeof(init_val);                                  \
        attr.max_len = 27;                                                 \
        attr.p_value = &init_val;                                          \
                                                                           \
        err = sd_ble_gatts_characteristic_add(m_service_handle,            \
            &char_md, &attr,                                               \
            &_handles);                                                    \
        APP_ERROR_CHECK(err);                                              \
    } while (0)

    ADD_CHAR(CHAR_UUID_SHORT, m_char_handles_auth);
    ADD_CHAR(CHAR_UUID_TIMESTAMP, m_char_handles_ts);
    ADD_CHAR(CHAR_UUID_IMAGE, m_char_handles_img);

#undef ADD_CHAR
}

static void advertising_init(void)
{
    ble_advdata_uuid_list_t adv_uuids;
    ble_uuid_t uuid = {SERVICE_UUID_SHORT, m_uuid_type};
    adv_uuids.uuid_cnt = 1;
    adv_uuids.p_uuids = &uuid;

    ble_advertising_init_t init = {0};
    init.advdata.name_type = BLE_ADVDATA_FULL_NAME;
    init.advdata.flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    init.advdata.uuids_complete = adv_uuids;
    init.config.ble_adv_fast_enabled = true;
    init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
    init.config.ble_adv_fast_timeout = APP_ADV_DURATION;

    init.evt_handler = on_adv_evt;

    APP_ERROR_CHECK(ble_advertising_init(&m_adv, &init));
    ble_advertising_conn_cfg_tag_set(&m_adv, APP_BLE_CONN_CFG_TAG);
}

static void advertising_start(void)
{
    APP_ERROR_CHECK(ble_advertising_start(&m_adv, BLE_ADV_MODE_FAST));
}

static void ble_evt_handler(ble_evt_t const *evt, void *ctx)
{
    switch (evt->header.evt_id)
    {
    case BLE_GAP_EVT_CONNECTED:
        printf("BLE Connected");
        NRF_LOG_INFO("Connected");
        m_conn_handle = evt->evt.gap_evt.conn_handle;
        break;

    case BLE_GAP_EVT_DISCONNECTED:
        printf("BLE disconnected");
        NRF_LOG_INFO("Disconnected");
        m_conn_handle = BLE_CONN_HANDLE_INVALID;
        // advertising_start();
        break;

    case BLE_GATTS_EVT_WRITE:
    {
        const ble_gatts_evt_write_t *w = &evt->evt.gatts_evt.params.write;

        if (w->handle == m_char_handles_auth.value_handle)
        {
            NRF_LOG_INFO("Auth CHR, %u bytes", w->len);
        }
        else if (w->handle == m_char_handles_ts.value_handle)
        {
            NRF_LOG_INFO("Timestamp CHR, %u bytes", w->len);
        }
        else if (w->handle == m_char_handles_img.value_handle)
        {
            if (w->data == NULL || w->len == 0)
            {
                NRF_LOG_WARNING("Invalid write to image characteristic");
                return;
            }

            if (receive_callback == NULL)
            {
                NRF_LOG_WARNING("receive_callback is NULL");
                return;
            }

            NRF_LOG_INFO("Image CHR write: %u bytes", w->len);
            image_receiver_handle_chunk(w->data, w->len, receive_callback);
        }
    }
    break;

    case BLE_GAP_EVT_DATA_LENGTH_UPDATE_REQUEST:
    {
        ble_gap_data_length_params_t params = {0};
        ret_code_t err_code = sd_ble_gap_data_length_update(evt->evt.gap_evt.conn_handle, &params, NULL);
        APP_ERROR_CHECK(err_code);
    }
    break;

    case BLE_GATTS_EVT_EXCHANGE_MTU_REQUEST:
    {
        uint16_t mtu = NRF_SDH_BLE_GATT_MAX_MTU_SIZE;
        ret_code_t err_code = sd_ble_gatts_exchange_mtu_reply(evt->evt.gatts_evt.conn_handle, mtu);
        APP_ERROR_CHECK(err_code);
    }
    break;

    default:
        break;
    }
}

static void ble_stack_init(void)
{
    uint32_t ram_start = 0;

    APP_ERROR_CHECK(nrf_sdh_enable_request());
    APP_ERROR_CHECK(nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start));
    APP_ERROR_CHECK(nrf_sdh_ble_enable(&ram_start));

    NRF_SDH_BLE_OBSERVER(m_ble_obs,
        APP_BLE_OBSERVER_PRIO,
        ble_evt_handler,
        NULL);
}

void ble_api_init(frame_callback_t callback)
{
    image_receiver_init(DEVICE_ID);

    char h[7];
    hash6(DEVICE_ID, h);
    snprintf(m_device_name, sizeof(m_device_name), "HOLYX-%.6s", h);
    printf("Device name: %s", (uint32_t)m_device_name);

    logs_init();
    APP_ERROR_CHECK(app_timer_init());
    power_init();

    ble_stack_init();
    gap_params_init();
    gatt_init();
    service_init();
    advertising_init();

    if (callback)
        receive_callback = callback;

    NRF_LOG_INFO("BLE firmware ready, advertising …");
    advertising_start();
}

void ble_api_process(void)
{
    if (!NRF_LOG_PROCESS())
        nrf_pwr_mgmt_run();
}