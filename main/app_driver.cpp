/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <device.h>
#include <esp_matter.h>
#include <led_driver.h>
#include "driver/gpio.h"

#include <app_priv.h>

#include <stdio.h>
#include "driver/ledc.h"
#include "esp_err.h"

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz
#define LEDC_DUTY_MAX           8192

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
extern uint16_t light_endpoint_id;
extern uint16_t light_endpoint_id_1;

#define MY_LED_NUM 1
#define MAX_LEDS 2

typedef struct {
    gpio_num_t gpio;
    ledc_channel_t channel;
    ledc_timer_t timer;
    bool power_state;
    uint32_t brightness;
} led_config_t;

led_config_t led_configs[MAX_LEDS] = {
    {
        .gpio = (gpio_num_t)CONFIG_EXAMPLE_LED_GPIO,
        .channel = LEDC_CHANNEL_0,
        .timer = LEDC_TIMER_0,
        .power_state = false,
        .brightness = 0
    },
    {
        .gpio = (gpio_num_t)CONFIG_EXAMPLE_LED2_GPIO,
        .channel = LEDC_CHANNEL_1,
        .timer = LEDC_TIMER_1,
        .power_state = false,
        .brightness = 0
    }
};

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_power(led_config_t *led_config, esp_matter_attr_val_t *val)
{
    ESP_LOGI(TAG, "Setting GPIO %d to %s", led_config->gpio, val->val.b ? "ON" : "OFF");
    
    led_config->power_state = val->val.b;
    uint32_t duty = led_config->power_state ? (led_config->brightness) : 0;
    
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel));

    return ESP_OK;
}

static esp_err_t app_driver_light_set_brightness(led_config_t *led_config, esp_matter_attr_val_t *val)
{
    int value = REMAP_TO_RANGE(val->val.u8, MATTER_BRIGHTNESS, LEDC_DUTY_MAX);
    ESP_LOGI(TAG, "Setting GPIO %d brightness to %d", led_config->gpio, value);
    
    led_config->brightness = value;
    if (led_config->power_state) {
        uint32_t duty = led_config->brightness;
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, led_config->channel, duty));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, led_config->channel));
    }

    return ESP_OK;
}

// static void app_driver_button_toggle_cb(void *arg, void *data)
// {
//     ESP_LOGI(TAG, "Toggle button pressed");
//     uint16_t endpoint_id = light_endpoint_id;
//     uint32_t cluster_id = OnOff::Id;
//     uint32_t attribute_id = OnOff::Attributes::OnOff::Id;
//
//     attribute_t *attribute = attribute::get(endpoint_id, cluster_id, attribute_id);
//
//     esp_matter_attr_val_t val = esp_matter_invalid(NULL);
//     attribute::get_val(attribute, &val);
//     val.val.b = !val.val.b;
//     attribute::update(endpoint_id, cluster_id, attribute_id, &val);
// }

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;
    led_config_t *led_configs = (led_config_t *)driver_handle;
    ESP_LOGI(TAG, "Enpoint id %d", endpoint_id);
    ESP_LOGI(TAG, "Light endpoint id %d", light_endpoint_id);

    int led_index = endpoint_id - 1;  // Assuming endpoint IDs start from 1
    if (led_index >= 0 && led_index < MAX_LEDS) {
        if (cluster_id == OnOff::Id) {
            if (attribute_id == OnOff::Attributes::OnOff::Id) {
                err = app_driver_light_set_power(&led_configs[led_index], val);
            }
        } else if (cluster_id == LevelControl::Id) {
            if (attribute_id == LevelControl::Attributes::CurrentLevel::Id) {
                err = app_driver_light_set_brightness(&led_configs[led_index], val);
            }
        }
    }
    return err;
}

// probably not working anymore
// esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
// {
//     esp_err_t err = ESP_OK;
//     void *priv_data = endpoint::get_priv_data(endpoint_id);
//     led_driver_handle_t handle = (led_driver_handle_t)priv_data;
//     esp_matter_attr_val_t val = esp_matter_invalid(NULL);
//
//     /* Setting power */
//     attribute_t * attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
//     attribute::get_val(attribute, &val);
//     // err |= app_driver_light_set_power(handle, &val);
//     err |= app_driver_light_set_power((led_config_t*)handle, &val);
//
//     /* Setting brightness */
//     attribute = attribute::get(endpoint_id, LevelControl::Id, LevelControl::Attributes::CurrentLevel::Id);
//     attribute::get_val(attribute, &val);
//     // err |= app_driver_light_set_brightness(handle, &val);
//     err |= app_driver_light_set_brightness((led_config_t*)handle, &val);
//
//
//     return err;
// }

app_driver_handle_t app_driver_light_init()
{
    for (int i = 0; i < MAX_LEDS; i++) {
        ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .duty_resolution = LEDC_TIMER_13_BIT,
            .timer_num = led_configs[i].timer,
            .freq_hz = 5000,
            .clk_cfg = LEDC_AUTO_CLK
        };
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        ledc_channel_config_t ledc_channel = {
            .gpio_num = led_configs[i].gpio,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = led_configs[i].channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = led_configs[i].timer,
            .duty = 0,
            .hpoint = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    return (app_driver_handle_t)led_configs;
}


// app_driver_handle_t app_driver_button_init()
// {   
//     /* Initialize button */
//     button_config_t config = button_driver_get_config();
//     button_handle_t handle = iot_button_create(&config);
//     iot_button_register_cb(handle, BUTTON_PRESS_DOWN, app_driver_button_toggle_cb, NULL);
//     return (app_driver_handle_t)handle;
// }
