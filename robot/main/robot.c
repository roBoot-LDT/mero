#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "mero";

// L298N control pings
#define IN1 GPIO_NUM_12
#define IN2 GPIO_NUM_11
#define IN3 GPIO_NUM_16
#define IN4 GPIO_NUM_17

static void gpio_init_outputs(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << IN1) | (1ULL << IN2) |
                        (1ULL << IN3) | (1ULL << IN4),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(IN1, 0);
    gpio_set_level(IN2, 0);
    gpio_set_level(IN3, 0);
    gpio_set_level(IN4, 0);
}

// dir: 1 forward, -1 back, 0 stop
static void motor_a(int dir) {
    gpio_set_level(IN1, dir > 0 ? 1 : 0);
    gpio_set_level(IN2, dir < 0 ? 1 : 0);
}

static void motor_b(int dir) {
    gpio_set_level(IN3, dir > 0 ? 1 : 0);
    gpio_set_level(IN4, dir < 0 ? 1 : 0);
}

static void stop_all(void){
    motor_a(0);
    motor_b(0);
}

void app_main(void){
    ESP_LOGI(TAG, "boot");
    gpio_init_outputs();
    stop_all();

    ESP_LOGI(TAG, "self-test cycle: forward / back / left / right");

    while (1) {
        ESP_LOGI(TAG, "forward");
        motor_a(1); motor_b(1);
        vTaskDelay(pdMS_TO_TICKS(2000));

        stop_all();
        vTaskDelay(pdMS_TO_TICKS(1000));

        ESP_LOGI(TAG, "back");
        motor_a(-1); motor_b(-1);
        vTaskDelay(pdMS_TO_TICKS(2000));

        stop_all();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "left");
        motor_a(-1); motor_b(1);
        vTaskDelay(pdMS_TO_TICKS(2000));

        stop_all();
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        ESP_LOGI(TAG, "right");
        motor_a(1); motor_b(-1);
        vTaskDelay(pdMS_TO_TICKS(2000));

        stop_all();
        vTaskDelay(pdMS_TO_TICKS(2000));
        
    }
}
