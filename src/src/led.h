#pragma once

#include <stdint.h>
#include <esp_err.h>


typedef struct {
    void *priv;
    int gpio_pin;
    uint32_t num_leds;
} led_t;

typedef uint32_t color_t;

#define COLOR(red, green, blue)  (                 \
                                  ((red << 16) & 0xff0000) | \
                                  ((green << 8) & 0x00ff00) | \
                                  (blue & 0x0000ff) )

#define COLOR_RED   COLOR(255, 0, 0)
#define COLOR_GREEN COLOR(0, 255, 0)
#define COLOR_BLUE  COLOR(0, 0, 255)
#define COLOR_WHITE COLOR(255, 255, 255)
#define COLOR_BLACK COLOR(0, 0, 0)

#define COLOR_GET_RED(color)    ((color >> 16) & 0xff)
#define COLOR_GET_GREEN(color)  ((color >> 8) & 0xff)
#define COLOR_GET_BLUE(color)   ((color) & 0xff)

esp_err_t led_init(led_t *led, unsigned int gpio_pin, unsigned int number_of_leds);
esp_err_t led_set_num_leds(led_t *led, uint32_t num_leds);
esp_err_t led_refresh_all(led_t *led, color_t color);
esp_err_t led_set(led_t *led, uint32_t idx_start, uint32_t num, color_t color);
esp_err_t led_refresh(led_t *led);
esp_err_t led_off(led_t *led);
