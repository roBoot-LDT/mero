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
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

static const char *TAG = "mero-robot";

#define IN1 GPIO_NUM_18
#define IN2 GPIO_NUM_17
#define IN3 GPIO_NUM_16
#define IN4 GPIO_NUM_15

#define DEVICE_NAME "mero-robot"

static uint8_t own_addr_type;

static void gpio_init(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL<<IN1)|(1ULL<<IN2)|(1ULL<<IN3)|(1ULL<<IN4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
    gpio_set_level(IN1,0); gpio_set_level(IN2,0);
    gpio_set_level(IN3,0); gpio_set_level(IN4,0);
}

static void motor_a(int dir) {
    gpio_set_level(IN1, dir > 0 ? 1 : 0);
    gpio_set_level(IN2, dir < 0 ? 1 : 0);
}

static void motor_b(int dir) {
    gpio_set_level(IN3, dir > 0 ? 1 : 0);
    gpio_set_level(IN4, dir < 0 ? 1 : 0);
}

static void handle_cmd(char c) {
    switch (c) {
        case 'f': motor_a( 1); motor_b( 1); ESP_LOGI(TAG,"forward"); break;
        case 'b': motor_a(-1); motor_b(-1); ESP_LOGI(TAG,"back");    break;
        case 'l': motor_a(1); motor_b( -1); ESP_LOGI(TAG,"left");    break;
        case 'r': motor_a(-1); motor_b(1); ESP_LOGI(TAG,"right");   break;
        case 's': motor_a( 0); motor_b( 0); ESP_LOGI(TAG,"stop");    break;
        default: break;
    }
}

// GATT характеристика для приёма команд
static int cmd_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        if (ctxt->om->om_len > 0) {
            ESP_LOGI(TAG, "received: %c", (char)ctxt->om->om_data[0]);
            handle_cmd((char)ctxt->om->om_data[0]);
        }
    }
    return 0;
}

// UUID сервиса и характеристики
static const ble_uuid128_t svc_uuid =
    BLE_UUID128_INIT(0x12,0x34,0x56,0x78,0x12,0x34,0x12,0x34,
                     0x12,0x34,0x12,0x34,0x12,0x34,0x56,0x78);
static const ble_uuid128_t cmd_chr_uuid =
    BLE_UUID128_INIT(0x12,0x34,0x56,0x78,0x12,0x34,0x12,0x34,
                     0x12,0x34,0x12,0x34,0x12,0x34,0x56,0x79);

static const struct ble_gatt_svc_def gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &cmd_chr_uuid.u,
                .access_cb = cmd_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 }
        },
    },
    { 0 }
};

static void start_advertising(void);

static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "client connected");
            // Обновляем параметры соединения для стабильности
            struct ble_gap_upd_params params = {
                .itvl_min            = 40,   // 50мс
                .itvl_max            = 80,   // 100мс
                .latency             = 0,
                .supervision_timeout = 1000, // 10 секунд
                .min_ce_len          = 0,
                .max_ce_len          = 0,
            };
            ble_gap_update_params(event->connect.conn_handle, &params);
        } else {
            start_advertising();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        motor_a(0); motor_b(0);
        ESP_LOGI(TAG, "client disconnected");
        start_advertising();
        break;
    default: break;
    }
    return 0;
}

static void start_advertising(void) {
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                      &adv_params, gap_event_handler, NULL);
    ESP_LOGI(TAG, "advertising started");
}

static void on_sync(void) {
    ble_hs_id_infer_auto(0, &own_addr_type);
    start_advertising();
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
    gpio_init();

    nimble_port_init();
    ble_hs_cfg.sync_cb  = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svcs);
    ble_gatts_add_svcs(gatt_svcs);
    ble_svc_gap_device_name_set(DEVICE_NAME);

    nimble_port_freertos_init(nimble_host_task);
    ESP_LOGI(TAG, "BLE NimBLE server started");
}