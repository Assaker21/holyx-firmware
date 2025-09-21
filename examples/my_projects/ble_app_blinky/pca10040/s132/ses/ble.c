/**
 * Copyright ...
 * (same license header)
 */

#include "ble.h"
#include <string.h>
#include <stdio.h>
#include "ble_types.h"
#include "nrf.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "ble_gap.h"
#include "ble_conn_params.h"

#include "ble_gatts.h"
#include "app_error.h"
#include "app_timer.h"
#include "sdk_config.h"

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

#define DEVICE_NAME                     "MinimalBLE"          /**< Device name shown in BLE advertising. */

#define APP_BLE_CONN_CFG_TAG           1                     /**< A tag for SoftDevice BLE configuration. */
#define APP_BLE_OBSERVER_PRIO          3                     /**< BLE event observer priority. */

#define APP_ADV_INTERVAL               64                    /**< Advertising interval (in 0.625 ms units). */
#define APP_ADV_DURATION               0                     /**< Advertising duration (0 = no timeout). */

#define MIN_CONN_INTERVAL              MSEC_TO_UNITS(100, UNIT_1_25_MS)
#define MAX_CONN_INTERVAL              MSEC_TO_UNITS(200, UNIT_1_25_MS)
#define SLAVE_LATENCY                  0
#define CONN_SUP_TIMEOUT               MSEC_TO_UNITS(4000, UNIT_10_MS)

#define FIRST_CONN_PARAMS_UPDATE_DELAY APP_TIMER_TICKS(20000) /**< Delay before starting connection param negotiations. */
#define NEXT_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)  /**< Subsequent delay. */
#define MAX_CONN_PARAMS_UPDATE_COUNT   3

// Simple custom service/characteristic
#define CUSTOM_SERVICE_UUID_BASE       {0x23, 0xCD, 0xAB, 0xEF, 0x5F, 0x78, 0x23, 0x15, \
                                        0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00}
#define CUSTOM_SERVICE_UUID            0x1523
#define CUSTOM_CHAR_UUID               0x1526

// -----------------------------------------------------------------------------
// Global / static variables
// -----------------------------------------------------------------------------

static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;

static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;
static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];
static uint8_t m_enc_scan_response_data[BLE_GAP_ADV_SET_DATA_SIZE_MAX];

NRF_BLE_GATT_DEF(m_gatt);  /**< GATT module instance. */

// The handles for our custom service and characteristic
static uint16_t                 m_service_handle;
static ble_gatts_char_handles_t m_char_handles;

// Advertising data buffer
static ble_gap_adv_data_t m_adv_data =
{
    .adv_data =
    {
        .p_data = m_enc_advdata,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    },
    .scan_rsp_data =
    {
        .p_data = m_enc_scan_response_data,
        .len    = BLE_GAP_ADV_SET_DATA_SIZE_MAX
    }
};

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context);
static void on_write(ble_gatts_evt_write_t const *p_evt_write);

// -----------------------------------------------------------------------------
// BLE Stack Initialization
// -----------------------------------------------------------------------------
static void ble_stack_init(void)
{
    ret_code_t err_code;

    // Initialize the SoftDevice
    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using default settings
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register our BLE event handler
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}

// -----------------------------------------------------------------------------
// GAP (Generic Access Profile) setup
// -----------------------------------------------------------------------------
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    // No security
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    // Set the device name
    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    // Connection parameters
    memset(&gap_conn_params, 0, sizeof(gap_conn_params));
    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}

// -----------------------------------------------------------------------------
// GATT Initialization
// -----------------------------------------------------------------------------
static void gatt_init(void)
{
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
    APP_ERROR_CHECK(err_code);
}

// -----------------------------------------------------------------------------
// Custom Service and Characteristic
// -----------------------------------------------------------------------------
static void custom_service_init(void)
{
    ret_code_t    err_code;
    ble_uuid_t    ble_uuid;
    ble_uuid128_t base_uuid = {CUSTOM_SERVICE_UUID_BASE};

    // Add a custom base UUID
    err_code = sd_ble_uuid_vs_add(&base_uuid, &ble_uuid.type);
    APP_ERROR_CHECK(err_code);

    ble_uuid.uuid = CUSTOM_SERVICE_UUID;

    // Add the service
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &m_service_handle);
    APP_ERROR_CHECK(err_code);

    // Add a custom characteristic
    ble_add_char_params_t char_params;
    memset(&char_params, 0, sizeof(char_params));

    char_params.uuid              = CUSTOM_CHAR_UUID;
    char_params.uuid_type         = ble_uuid.type;
    char_params.init_len          = 1;
    char_params.max_len           = 20;  // typical small MTU demonstration
    char_params.char_props.read   = 1;
    char_params.char_props.write  = 1;
    char_params.char_props.write_wo_resp = 1;

    char_params.read_access       = SEC_OPEN;
    char_params.write_access      = SEC_OPEN;

    err_code = characteristic_add(m_service_handle, &char_params, &m_char_handles);
    APP_ERROR_CHECK(err_code);
}

// -----------------------------------------------------------------------------
// Advertising Initialization
// -----------------------------------------------------------------------------
static void advertising_init(void)
{
    ret_code_t    err_code;
    ble_advdata_t advdata;
    ble_advdata_t srdata;

    // Build advertising data
    memset(&advdata, 0, sizeof(advdata));
    advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    // We won't include any service UUID in the adv, so keep srdata minimal too
    memset(&srdata, 0, sizeof(srdata));

    // Encode the advertising data
    err_code = ble_advdata_encode(&advdata,
                                  m_adv_data.adv_data.p_data,
                                  &m_adv_data.adv_data.len);
    APP_ERROR_CHECK(err_code);

    // Encode the scan response data
    err_code = ble_advdata_encode(&srdata,
                                  m_adv_data.scan_rsp_data.p_data,
                                  &m_adv_data.scan_rsp_data.len);
    APP_ERROR_CHECK(err_code);

    // Set advertising parameters
    ble_gap_adv_params_t adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.primary_phy     = BLE_GAP_PHY_1MBPS;
    adv_params.duration        = APP_ADV_DURATION;
    adv_params.properties.type = BLE_GAP_ADV_TYPE_CONNECTABLE_SCANNABLE_UNDIRECTED;
    adv_params.p_peer_addr     = NULL;
    adv_params.filter_policy   = BLE_GAP_ADV_FP_ANY;
    adv_params.interval        = APP_ADV_INTERVAL;

    // Configure the advertising set
    err_code = sd_ble_gap_adv_set_configure(&m_adv_handle, &m_adv_data, &adv_params);
    APP_ERROR_CHECK(err_code);
}

// -----------------------------------------------------------------------------
// Connection Parameters
// -----------------------------------------------------------------------------
static void on_conn_params_evt(ble_conn_params_evt_t *p_evt)
{
    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        // Disconnect if parameters can't be negotiated
        sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
    }
}

static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}

static void conn_params_init(void)
{
    ble_conn_params_init_t cp_init;
    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = true;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    ret_code_t err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}

// -----------------------------------------------------------------------------
// BLE Event Handler
// -----------------------------------------------------------------------------
static void ble_evt_handler(ble_evt_t const *p_ble_evt, void *p_context)
{
    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            printf("BLE connected.\r\n");
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            printf("BLE disconnected.\r\n");
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
            // Restart advertising so it can connect again
            ble_start_advertising();
            break;

        case BLE_GATTS_EVT_WRITE:
        {
            ble_gatts_evt_write_t const *p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

            // If the data is written to our custom characteristic
            if (p_evt_write->handle == m_char_handles.value_handle)
            {
                on_write(p_evt_write);
            }
        } break;

        default:
            // No implementation needed
            break;
    }
}

// -----------------------------------------------------------------------------
// Handle incoming write to custom characteristic
// -----------------------------------------------------------------------------
static void on_write(ble_gatts_evt_write_t const *p_evt_write)
{
    uint16_t length = p_evt_write->len;

    // Print out the data
    printf("Data received on custom char (len=%d): ", length);
    for (uint16_t i = 0; i < length; i++)
    {
        printf("%02X ", p_evt_write->data[i]);
    }
    printf("\r\n");
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
void ble_init(void)
{
    ble_stack_init();
    gap_params_init();
    gatt_init();
    custom_service_init();
    advertising_init();
    conn_params_init();
}

void ble_start_advertising(void)
{
    sd_ble_gap_adv_start(m_adv_handle, APP_BLE_CONN_CFG_TAG);
    printf("Advertising started.\r\n");
}
