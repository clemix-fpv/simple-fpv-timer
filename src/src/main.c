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


static const char * TAG = "main";

StaticSemaphore_t xMutexBuffer;
static ctx_t ctx = {0};

void app_main() {

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

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

    task_rssi_init(&ctx);

    while (ctx.gui) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_ERROR_CHECK(gui_stop(&ctx));
}
