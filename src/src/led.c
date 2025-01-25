// SPDX-License-Identifier: GPL-3.0-or-later

#include "led.h"
#include "esp_err.h"
#include "esp_log.h"
#include "led_strip.h"
#include <string.h>

static const char *TAG = "LED";

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

static led_strip_handle_t led_handle(led_t *led) {
    return (led_strip_handle_t)led->priv;
}

esp_err_t led_init(led_t *led, unsigned int gpio_pin,
                   unsigned int number_of_leds) {
    esp_err_t e;

    if (!led)
        return ESP_ERR_INVALID_ARG;

    memset(led, 0, sizeof(led_t));

    led->gpio_pin = gpio_pin;
    led->num_leds = number_of_leds;

    led_strip_config_t strip_config = {.strip_gpio_num = led->gpio_pin,
        .max_leds = led->num_leds,
        .led_model = LED_MODEL_WS2812,
        .color_component_format =
        LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        }};

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to
        // different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .mem_block_symbols =
        64, // the memory size of each RMT channel, in words (4 bytes)
        .flags = {
            //            .with_dma = true, // DMA feature is available on chips
            //            like ESP32-S3/P4
            .with_dma =
            true, // DMA feature is available on chips like ESP32-S3/P4
        }};

    // LED Strip object handle
    led_strip_handle_t led_strip;
    e = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);

    if (e == ESP_ERR_NOT_SUPPORTED) {
        rmt_config.flags.with_dma = false;

        ESP_LOGW(TAG, "LED creation failed with DMA failed, retry without DMA!");
        e = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    }
    if (e != ESP_OK)
        return e;

    led->priv = led_strip;

    ESP_LOGW(TAG, "LED created RMT backend%s leds:%" PRIu32 " GPIO:%d",
             rmt_config.flags.with_dma ? " (DMA)" : "", strip_config.max_leds,
             strip_config.strip_gpio_num);
    return e;
}

esp_err_t led_refresh_all(led_t *led, color_t color) {
    esp_err_t e;
    if ((e = led_set(led, 0, -1U, color)) != ESP_OK)
        return e;
    if ((e = led_refresh(led)) != ESP_OK)
        return e;
    return ESP_OK;
}

esp_err_t led_set(led_t *led, uint32_t idx_start, uint32_t num, color_t color) {
    esp_err_t e;
    led_strip_handle_t handle = led_handle(led);
    uint32_t red = COLOR_GET_RED(color);
    uint32_t green = COLOR_GET_GREEN(color);
    uint32_t blue = COLOR_GET_BLUE(color);

    if (num > led->num_leds)
        num = led->num_leds;

    for (uint32_t i = 0; i < num; i++) {
        if ((e = led_strip_set_pixel(handle, (i + idx_start) % led->num_leds, red,
                                     green, blue)) != ESP_OK) {
            return e;
        }
    }

    return ESP_OK;
}

esp_err_t led_off(led_t *led)
{
    return led_strip_clear(led_handle(led));
}

esp_err_t led_refresh(led_t *led)
{
    return led_strip_refresh(led_handle(led));
}

esp_err_t led_set_num_leds(led_t *led, uint32_t num_leds)
{
    if (led->priv) {
        led_strip_del(led->priv);
        led->priv = NULL;
    }
    return led_init(led, led->gpio_pin, num_leds);
}
