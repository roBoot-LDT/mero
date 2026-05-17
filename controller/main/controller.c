#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

static const char *TAG = "mero-remote";

#define BTN_FORWARD  GPIO_NUM_26
#define BTN_BACK     GPIO_NUM_27
#define BTN_LEFT     GPIO_NUM_14
#define BTN_RIGHT    GPIO_NUM_12
#define BTN_STOP     GPIO_NUM_13

#define ROBOT_NAME   "mero-robot"

static uint8_t own_addr_type;
static uint16_t conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t cmd_attr_handle = 0;
static bool connected = false;

static void start_scan(void);
static int gap_event(struct ble_gap_event *event, void *arg);

static void send_cmd(char c) {
    if (!connected || cmd_attr_handle == 0) return;
    ble_gattc_write_no_rsp_flat(conn_handle, cmd_attr_handle, &c, 1);
}

static void buttons_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL<<BTN_FORWARD)|(1ULL<<BTN_BACK)|
                        (1ULL<<BTN_LEFT)|(1ULL<<BTN_RIGHT)|(1ULL<<BTN_STOP),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

static char last_cmd = 's';

static void buttons_task(void *arg) {
    while (1) {
        char cmd = 0;
        if      (!gpio_get_level(BTN_FORWARD)) cmd = 'f';
        else if (!gpio_get_level(BTN_BACK))    cmd = 'b';
        else if (!gpio_get_level(BTN_LEFT))    cmd = 'l';
        else if (!gpio_get_level(BTN_RIGHT))   cmd = 'r';
        else if (!gpio_get_level(BTN_STOP))    cmd = 's';
        else                                   cmd = 's';

        if (cmd != last_cmd) {
            last_cmd = cmd;
            send_cmd(cmd);
            ESP_LOGI(TAG, "cmd: %c", cmd);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// GATT discovery — ищем нашу характеристику
static int on_disc_chr(uint16_t conn_h, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == 0 && chr) {
        // Берём первую характеристику с правом записи
        if ((chr->properties & BLE_GATT_CHR_F_WRITE) ||
            (chr->properties & BLE_GATT_CHR_F_WRITE_NO_RSP)) {
            if (cmd_attr_handle == 0) {
                cmd_attr_handle = chr->val_handle;
                ESP_LOGI(TAG, "writable characteristic found, handle=%d", 
                         cmd_attr_handle);
                connected = true;
            }
        }
    }
    return 0;
}

static int on_disc_svc(uint16_t conn_h, const struct ble_gatt_error *error,
                       const struct ble_gatt_svc *svc, void *arg) {
    if (error->status == 0 && svc) {
        ble_gattc_disc_all_chrs(conn_h, svc->start_handle,
                                svc->end_handle, on_disc_chr, NULL);
    }
    return 0;
}

static void start_scan(void) {
    struct ble_gap_disc_params disc_params = {
        .itvl = 0,
        .window = 0,
        .filter_policy = 0,
        .limited = 0,
        .passive = 0,
        .filter_duplicates = 1,
    };
    ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, gap_event, NULL);
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        struct ble_hs_adv_fields fields;
        ble_hs_adv_parse_fields(&fields, event->disc.data,
                                event->disc.length_data);
        if (fields.name && fields.name_len == strlen(ROBOT_NAME) &&
            memcmp(fields.name, ROBOT_NAME, fields.name_len) == 0) {
            ESP_LOGI(TAG, "found mero-robot, connecting...");
            ble_gap_disc_cancel();
            ble_gap_connect(own_addr_type, &event->disc.addr,
                           5000, NULL, gap_event, NULL);
        }
        break;
    }
    case BLE_GAP_EVENT_CONNECT:
    if (event->connect.status == 0) {
        conn_handle = event->connect.conn_handle;
        ESP_LOGI(TAG, "connected");
        ble_gattc_disc_all_svcs(conn_handle, on_disc_svc, NULL);
    } else {
        ESP_LOGE(TAG, "connect failed, scanning again");
        start_scan();
    }
    break;
case BLE_GAP_EVENT_DISCONNECT:
    connected = false;
    cmd_attr_handle = 0;
    conn_handle = BLE_HS_CONN_HANDLE_NONE;
    ESP_LOGW(TAG, "disconnected, scanning again");
    start_scan();
    break;
    default: break;
    }
    return 0;
}



static void on_sync(void) {
    ble_hs_id_infer_auto(0, &own_addr_type);
    ESP_LOGI(TAG, "connecting to mero-robot by MAC...");
    
    ble_addr_t robot_addr = {
        .type = BLE_ADDR_PUBLIC,
        .val = {0xa6, 0x75, 0xc3, 0xa4, 0xb5, 0x80}  // MAC в обратном порядке
    };
    
    ble_gap_connect(own_addr_type, &robot_addr, 10000, NULL, gap_event, NULL);
}

static void on_reset(int reason) {
    ESP_LOGE(TAG, "BLE reset: %d", reason);
}

static void nimble_host_task(void *param) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void app_main(void) {
    ESP_LOGI(TAG, "boot");
    ESP_ERROR_CHECK(nvs_flash_init());
    buttons_init();

    nimble_port_init();
    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    nimble_port_freertos_init(nimble_host_task);
    xTaskCreate(buttons_task, "buttons", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "remote started");
}