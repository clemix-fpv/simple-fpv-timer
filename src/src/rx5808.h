// SPDX-License-Identifier: GPL-3.0+

#pragma  once

#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_adc/adc_oneshot.h"

typedef struct {
  int pin_mosi;
  int pin_clk;
  int pin_cs;
  int pin_rssi;

  spi_device_handle_t spi;
  adc_oneshot_unit_handle_t adc;
  adc_cali_handle_t adc_cali;
  bool adc_calibrated;
} rx5808_t;


esp_err_t rx5808_init(rx5808_t *handle, int mosi, int clk, int cs, int rssi);
esp_err_t rx5808_read_rssi(rx5808_t *handle, int *raw, int *voltage);
esp_err_t rx5808_set_channel(rx5808_t *handle, int freq);
