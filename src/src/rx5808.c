// SPDX-License-Identifier: GPL-3.0+

#include <string.h>
#include <freertos/FreeRTOS.h>
#include "esp_log.h"
#include "rx5808.h"

static const char * TAG = "rx5808";

#define RX5808_ADC_UNIT       ADC_UNIT_1
#define RX5808_ADC_ATTEN      ADC_ATTEN_DB_0
#define RX5808_ADC_BITWIDTH   ADC_BITWIDTH_DEFAULT


static bool rx5808_adc_calibration_init( adc_channel_t channel, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = RX5808_ADC_UNIT,
            .chan = channel,
            .atten = RX5808_ADC_ATTEN,
            .bitwidth = RX5808_ADC_BITWIDTH,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = RX5808_ADC_UNIT,
            .atten = RX5808_ADC_ATTEN,
            .bitwidth = RX5808_ADC_BITWIDTH,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}


esp_err_t rx5808_init(rx5808_t *handle, int mosi, int clk, int cs, int rssi) {

  esp_err_t ret;
  spi_transaction_t trans;

  memset(handle, 0, sizeof(*handle));
  handle->pin_mosi = mosi;
  handle->pin_rssi = rssi;
  handle->pin_cs = cs;
  handle->pin_clk = clk;

  spi_bus_config_t buscfg = {
    .miso_io_num = -1,
    .mosi_io_num = mosi,
    .sclk_io_num = clk,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
  };

  spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 10 * 1000 * 1000,     //Clock out at 10 MHz
    .mode = 0,                              //SPI mode 0
    .spics_io_num = cs,             //CS pin
    .queue_size = 1,                        //We want to be able to queue 7 transactions at a time
    .pre_cb = NULL,
    .flags = SPI_DEVICE_TXBIT_LSBFIRST,
  };
  //Initialize the SPI bus
  if ((ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO)) != ESP_OK)
    return ret;
  //Attach the LCD to the SPI bus
  if ((ret = spi_bus_add_device(SPI2_HOST, &devcfg, &handle->spi)) != ESP_OK)
    return ret;

  /* Send initialization sequence */
  memset(&trans, 0, sizeof(spi_transaction_t));
  trans.flags = SPI_TRANS_USE_TXDATA;

  trans.length = 25;
  trans.tx_data[0] = 0x10;
  trans.tx_data[1] = 0x01;
  trans.tx_data[2] = 0;
  trans.tx_data[3] = 0;//0b00010000;

  if ((ret = spi_device_transmit(handle->spi, &trans)) != ESP_OK)
    return ret;


  /* Prepare the adc-one-shot reading for RSSI values */
  adc_oneshot_unit_init_cfg_t init_config1 = {
  .unit_id = ADC_UNIT_1,
  };

  if ((ret = adc_oneshot_new_unit(&init_config1, &handle->adc)) != ESP_OK)
    return ret;

  adc_oneshot_chan_cfg_t adc_config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_0,
  };

  if ((ret = adc_oneshot_config_channel(handle->adc, rssi, &adc_config)) != ESP_OK)
    return ret;

  handle->adc_calibrated = rx5808_adc_calibration_init(rssi, &handle->adc_cali);

  return ESP_OK;
}


esp_err_t rx5808_read_rssi(rx5808_t *handle, int *raw, int *voltage)
{
  esp_err_t ret;
  int v = 0;

  if ((ret = adc_oneshot_read(handle->adc, handle->pin_rssi, &v)) != ESP_OK)
    return ret;

  if (raw)
    *raw = v;

  if (voltage && handle->adc_calibrated) {
    if ((ret = adc_cali_raw_to_voltage(handle->adc_cali, v, voltage)) != ESP_OK)
      return ret;
  }

  return ESP_OK;
}


esp_err_t rx5808_set_channel(rx5808_t *handle, int freq)
{
    static spi_transaction_t trans;

//    ESP_LOGI(TAG, "set frequency: %d", freq);

    memset(&trans, 0, sizeof(spi_transaction_t));
    trans.flags = SPI_TRANS_USE_TXDATA;

    trans.length = 25;

    uint32_t flsb = (freq - 479) / 2;

    uint8_t flsbH = flsb >> 5;
    uint8_t flsbL = flsb & 0x1F;

    trans.tx_data[0] = flsbL * 32 + 17;
    trans.tx_data[1] = flsbH * 16 + flsbL / 8;
    trans.tx_data[2] = flsbH / 16;
    trans.tx_data[3] = 0;

    return spi_device_transmit(handle->spi, &trans);
}
