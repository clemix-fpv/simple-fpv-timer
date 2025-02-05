// SPDX-License-Identifier: GPL-3.0+


#include <limits.h>
#include <freertos/FreeRTOS.h>
#include <stdint.h>
#include "esp_chip_info.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "config.h"
#include "timer.h"
#include "wifi.h"
#include "gui.h"
#include "captdns.h"
#include "rx5808.h"
#include "esp_timer.h"
#include "lwip/ip_addr.h"
#include "json.h"
#include "simple_fpv_timer.h"
#include "osd.h"
#include "task_rssi.h"
#include "task_led.h"
#include "driver/gpio.h"


static const char * TAG = "main";

StaticSemaphore_t xMutexBuffer;
static ctx_t ctx = {0};


void print_mem_usage()
{
  struct cap2name {
    unsigned int cap;
    const char * name;
  };
  static const struct cap2name caps[] =  {
    { MALLOC_CAP_8BIT,  "8Bit" },
    { MALLOC_CAP_EXEC,  "EXEC" },
    { MALLOC_CAP_32BIT, "32bit"},
    { MALLOC_CAP_8BIT,  "8bit"},
    { MALLOC_CAP_DMA,   "DMA" },
    { 0, NULL },
  };
  const struct cap2name *cap;


  for(cap = caps; cap->name; cap++){
//    heap_caps_print_heap_info(cap->cap);

    size_t total = heap_caps_get_total_size(cap->cap);
    size_t free = heap_caps_get_free_size(cap->cap);
    printf("MEMORY %7s used:%3u%% free:%5u total:%5u\n", cap->name,  ((total-free) * 100) / total, free, total );
  }
}


void factory_reset() {

    ctx.cfg.eeprom.magic[0]++;
    cfg_save(&ctx.cfg);
    sft_emit_led_blink(&ctx, COLOR_RED);
    vTaskDelay(pdMS_TO_TICKS(3000));
    esp_restart();
}


void app_main() {

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_set_level(GPIO_NUM_0, 1);

    ctx.sem = xSemaphoreCreateMutexStatic( &xMutexBuffer );
    esp_chip_info_t chip_info;
    ESP_LOGI(TAG, "Started\n");
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "Number of cores: %d",  chip_info.cores);

    ESP_ERROR_CHECK(cfg_load(&ctx.cfg));
    cfg_dump(&ctx.cfg);
    /*ctx.cfg.eeprom.wifi_mode = 0;*/

    wifi_setup(&ctx.wifi, &ctx.cfg.eeprom);
    /* Check if STA connection was successful, if not fallback to AP_MODE */
    if (ctx.wifi.state == WIFI_STA && !ctx.wifi.sta_connected) {
        ctx.cfg.eeprom.wifi_mode = CFG_WIFI_MODE_AP;
        cfg_generate_random_ssid(ctx.cfg.eeprom.ssid, sizeof(ctx.cfg.eeprom.ssid));
    }


    sft_init(&ctx);

    captdnsInit();
    ESP_ERROR_CHECK(gui_start(&ctx));

    osd_init(&ctx.osd, ctx.cfg.eeprom.elrs_uid);

    /* initialize timers */
    //timer_start(T_WEBSOCKET, 100000, update_websockets, &ctx);

    sft_update_settings(&ctx);
    cfg_eeprom_to_running(&ctx.cfg);

    task_led_init(&ctx);
    task_rssi_init(&ctx);

    int i = 0;
    int reset_cnt = 0;
    while (ctx.gui) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if ((i++  % 30) == 0)
            print_mem_usage();

        if (gpio_get_level(GPIO_NUM_0) == 0) {
            reset_cnt++;
            if(reset_cnt > 10) {
                factory_reset();
            }
        }

    }

    ESP_ERROR_CHECK(gui_stop(&ctx));
}
