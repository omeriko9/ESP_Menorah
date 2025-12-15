#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_system.h"

#define TAG "MENORAH"

// --- Configuration ---
#define BUTTON_GPIO     GPIO_NUM_0
#define SHAMASH_GPIO    GPIO_NUM_21
#define CANDLE_1_GPIO   GPIO_NUM_22
#define CANDLE_2_GPIO   GPIO_NUM_23
#define CANDLE_3_GPIO   GPIO_NUM_19
#define CANDLE_4_GPIO   GPIO_NUM_18
#define CANDLE_5_GPIO   GPIO_NUM_5
#define CANDLE_6_GPIO   GPIO_NUM_17
#define CANDLE_7_GPIO   GPIO_NUM_16
#define CANDLE_8_GPIO   GPIO_NUM_4

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_DUTY_RES           LEDC_TIMER_8_BIT // Set duty resolution to 8 bits
#define LEDC_FREQUENCY          5000             // Frequency in Hertz. Set frequency at 5 kHz

typedef struct {
    int gpio;
    ledc_mode_t mode;
    ledc_channel_t channel;
} led_config_t;

// Array of LED configurations. Index 0 is Shamash, 1-8 are candles.
const led_config_t led_configs[9] = {
    {SHAMASH_GPIO,  LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0}, // Shamash on HS Channel 0
    {CANDLE_1_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_0}, // Candles on LS Channels 0-7
    {CANDLE_2_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_1},
    {CANDLE_3_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_2},
    {CANDLE_4_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_3},
    {CANDLE_5_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_4},
    {CANDLE_6_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_5},
    {CANDLE_7_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_6},
    {CANDLE_8_GPIO, LEDC_LOW_SPEED_MODE,  LEDC_CHANNEL_7}
};

// Global state
uint8_t current_day = 1; // 1 to 8

// --- NVS Functions ---
void init_nvs() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void save_day(uint8_t day) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
        err = nvs_set_u8(my_handle, "day", day);
        if (err != ESP_OK) ESP_LOGE(TAG, "Failed to write day to NVS");
        
        err = nvs_commit(my_handle);
        if (err != ESP_OK) ESP_LOGE(TAG, "Failed to commit NVS");
        
        nvs_close(my_handle);
        ESP_LOGI(TAG, "Saved day: %d", day);
    }
}

uint8_t load_day() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    uint8_t day = 1; // Default
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
    } else {
        err = nvs_get_u8(my_handle, "day", &day);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGI(TAG, "The value is not initialized yet!");
            day = 1;
        } else if (err != ESP_OK) {
            ESP_LOGE(TAG, "Error (%s) reading!", esp_err_to_name(err));
        }
        nvs_close(my_handle);
    }
    if (day < 1 || day > 8) day = 1; // Safety check
    ESP_LOGI(TAG, "Loaded day: %d", day);
    return day;
}

// --- LED Control ---
void init_leds() {
    // Configure Timer for Low Speed Mode
    ledc_timer_config_t ledc_timer_ls = {
        .speed_mode       = LEDC_LOW_SPEED_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_ls));

    // Configure Timer for High Speed Mode
    ledc_timer_config_t ledc_timer_hs = {
        .speed_mode       = LEDC_HIGH_SPEED_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_hs));

    // Prepare and then apply the LEDC PWM channel configuration
    for (int i = 0; i < 9; i++) {
        ledc_channel_config_t ledc_channel = {
            .speed_mode     = led_configs[i].mode,
            .channel        = led_configs[i].channel,
            .timer_sel      = LEDC_TIMER,
            .intr_type      = LEDC_INTR_DISABLE,
            .gpio_num       = led_configs[i].gpio,
            .duty           = 0, // Set duty to 0%
            .hpoint         = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }
}

// Task to simulate fire flicker
void flicker_task(void *pvParameter) {
    srand(time(NULL));
    while (1) {
        for (int i = 0; i < 9; i++) {
            // Determine if this LED should be on
            // Index 0 is Shamash (always on)
            // Index 1 is Candle 1 (on if day >= 1)
            // Index 8 is Candle 8 (on if day >= 8)
            bool is_on = (i == 0) || (i <= current_day);

            if (is_on) {
                // Random duty cycle between 100 and 255 for flicker effect
                // 255 is max brightness (8-bit)
                // 100 is dim but visible
                uint32_t duty = (rand() % 156) + 100; 
                ledc_set_duty(led_configs[i].mode, led_configs[i].channel, duty);
                ledc_update_duty(led_configs[i].mode, led_configs[i].channel);
            } else {
                ledc_set_duty(led_configs[i].mode, led_configs[i].channel, 0);
                ledc_update_duty(led_configs[i].mode, led_configs[i].channel);
            }
        }
        // Random delay to make flicker less uniform
        vTaskDelay((40 + (rand() % 20)) / portTICK_PERIOD_MS);
    }
}

// --- Button Handling ---
void init_button() {
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1; // Enable pull-up for Boot button
    gpio_config(&io_conf);
}

void app_main(void) {
    // 1. Initialize NVS and load state
    init_nvs();
    current_day = load_day();

    // 2. Initialize Hardware
    init_leds();
    init_button();

    // 3. Start Flicker Task
    xTaskCreate(flicker_task, "flicker_task", 2048, NULL, 5, NULL);

    ESP_LOGI(TAG, "Menorah started. Current day: %d", current_day);

    // 4. Main Loop - Button Polling
    int last_button_state = 1; // High (not pressed)
    
    while (1) {
        int button_state = gpio_get_level(BUTTON_GPIO);

        // Check for falling edge (press)
        if (last_button_state == 1 && button_state == 0) {
            // Debounce
            vTaskDelay(50 / portTICK_PERIOD_MS);
            if (gpio_get_level(BUTTON_GPIO) == 0) {
                // Valid press
                ESP_LOGI(TAG, "Button Pressed!");
                
                current_day++;
                if (current_day > 8) {
                    current_day = 1;
                }
                
                save_day(current_day);
                ESP_LOGI(TAG, "New day set: %d", current_day);

                // Wait for release to avoid multiple triggers
                while (gpio_get_level(BUTTON_GPIO) == 0) {
                    vTaskDelay(10 / portTICK_PERIOD_MS);
                }
            }
        }
        last_button_state = button_state;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
