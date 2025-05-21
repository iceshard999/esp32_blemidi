/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Includes */
#include "blemidi_svc.h"
#include "common.h"
//#include "led.h"
#include "esp_mac.h"

static int midi_data_callback(uint16_t conn_handle, uint16_t attr_handle, 
                             struct ble_gatt_access_ctxt *ctxt, void *arg);

static int cccd_callback(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg);


/* Private variables */

// BLE MIDI服务的UUID (128-bit格式)
static const ble_uuid128_t midi_service_uuid = 
    BLE_UUID128_INIT(0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7, 
                     0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03);

// MIDI特征值的UUID (128-bit格式)
static const ble_uuid128_t midi_char_uuid = 
    BLE_UUID128_INIT(0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1, 
                     0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77);

// 描述符UUID (Client Characteristic Configuration, CCCD)
//static const ble_uuid_t *cccd_uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16);
// MIDI特征值的缓冲区和句柄
static uint8_t midi_chr_val[512] = {0}; // Larger buffer for MIDI data
static uint16_t midi_chr_val_handle;

static uint16_t midi_chr_conn_handle = 0;
static bool midi_chr_conn_handle_inited = false;
static bool midi_ind_status = false;

// MIDI描述符的句柄
static uint16_t midi_cccd_handle = 0; // 新增


/* GATT services table */
// GATT服务表定义
static const struct ble_gatt_svc_def midi_service_table[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &midi_service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &midi_char_uuid.u,
                .access_cb = midi_data_callback,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
                .val_handle = &midi_chr_val_handle,
            },
            {0} // 结束特征列表
        }
    },
    {0} // 结束服务列表
};



/* Private functions */
// MIDI数据接收回调
static int midi_data_callback(uint16_t conn_handle, uint16_t attr_handle, 
                             struct ble_gatt_access_ctxt *ctxt, void *arg) {
    int rc = 0;
    struct os_mbuf *om = ctxt->om;

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            rc = os_mbuf_append(om, midi_chr_val, sizeof(midi_chr_val));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            if (om->om_len > sizeof(midi_chr_val)) {
                return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }

            uint16_t len = OS_MBUF_PKTLEN(om);
            rc = ble_hs_mbuf_to_flat(om, midi_chr_val, len, NULL);
            if (rc != 0) {
                return BLE_ATT_ERR_UNLIKELY;
            }

            ESP_LOGI(TAG, "Received MIDI data (len=%d): ", len);
            for (int i = 0; i < len; i++) {
                ESP_LOGI(TAG, "%02X ", midi_chr_val[i]);
            }
            return 0;

        default:
            return BLE_ATT_ERR_UNLIKELY;
    }
}


/* Public functions */
void send_midi_note_on(uint8_t note, uint8_t velocity) {
    if (!midi_chr_conn_handle_inited) {
        //ESP_LOGE(TAG, "MIDI characteristic connection handle not initialized");
        return;
    }
    uint8_t midi_msg[5] = {0x80, 0x80, 0x90, note, velocity}; // MIDI Note On消息
    struct os_mbuf *om = ble_hs_mbuf_from_flat(midi_msg, sizeof(midi_msg));
    ble_gatts_notify_custom(midi_chr_conn_handle, midi_chr_val_handle, om);
    //ESP_LOGI(TAG, "Note On: Note=%d, Velocity=%d", note, velocity);
}

void send_midi_note_off(uint8_t note) {
    if (!midi_chr_conn_handle_inited) {
        //ESP_LOGE(TAG, "MIDI characteristic connection handle not initialized");
        return;
    }
    uint8_t midi_msg[5] = {0x80, 0x80, 0x80, note, 0}; // MIDI Note Off消息
    struct os_mbuf *om = ble_hs_mbuf_from_flat(midi_msg, sizeof(midi_msg));
    ble_gatts_notify_custom(midi_chr_conn_handle, midi_chr_val_handle, om);
    //ESP_LOGI(TAG, "Note Off: Note=%d", note);
}

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGI(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGI(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGI(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        // 判断是不是CCCD
        if (ctxt->dsc.dsc_def->att_flags & (BLE_ATT_F_READ | BLE_ATT_F_WRITE)) {
            midi_cccd_handle = ctxt->dsc.handle;
        }
        break;

    /* Unknown event */
    default:
        assert(0);
        break;
    }
}


/*
 *  GATT server subscribe event callback
 *      1. Update MIDI subscription status on connect/disconnect
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    /* Check attribute handle */
    ESP_LOGI(TAG, "subscribe event; attr_handle=%d; midi_chr_val_handle=%d; "
                 "midi_cccd_handle=%d",
             event->subscribe.attr_handle, midi_chr_val_handle, midi_cccd_handle);
    if (event->subscribe.attr_handle == midi_chr_val_handle || event->subscribe.attr_handle == midi_cccd_handle) {
        /* Update MIDI subscription status */
        midi_chr_conn_handle = event->subscribe.conn_handle;
        midi_chr_conn_handle_inited = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "MIDI notifications %s", 
                 event->subscribe.cur_notify ? "enabled" : "disabled");
    }
}
/*
void gatt_svr_unsubscribe_cb(struct ble_gap_event *event) {
    // Check connection handle 
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "unsubscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "unsubscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }
    if (event->subscribe.attr_handle == midi_chr_val_handle) {
        midi_chr_conn_handle = 0;
        midi_chr_conn_handle_inited = false;
        midi_ind_status = event->subscribe.cur_indicate;
    }
}

 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void) {
    /* Local variables */
    int rc;

    /* 1. GATT service initialization */
    ble_svc_gatt_init();

    /* 2. Update GATT services counter */
    rc = ble_gatts_count_cfg(midi_service_table);
    if (rc != 0) {
        return rc;
    }

    /* 3. Add GATT services */
    rc = ble_gatts_add_svcs(midi_service_table);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
