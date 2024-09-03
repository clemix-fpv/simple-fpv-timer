
#include <limits.h>
#include <freertos/FreeRTOS.h>
#include <stdint.h>
#include "esp_chip_info.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "config.h"
#include "wifi.h"
#include "gui.h"
#include "captdns.h"
#include "rx5808.h"
#include "esp_timer.h"
#include "lwip/ip_addr.h"
#include "json.h"
#include "simple_fpv_timer.h"
#include "osd.h"


static const char * TAG = "main";

StaticSemaphore_t xMutexBuffer;
static ctx_t ctx = {0};

#define PIN_NUM_MOSI 23
#define PIN_NUM_CLK  18
#define PIN_NUM_CS   5
#define PIN_RSSI     ADC_CHANNEL_6


#define TIMER_RESTART 0
#define TIMER_STOP    1
#define TIMER_BREAK_LOOP 2
struct timer_s {
    int64_t start;
    int64_t duration;
    int64_t end; /*  calculated end in milliseconds */
    int (*callback)(int64_t over_shoot, void *user_data);
    void * user_data;
};

void timer_restart(struct timer_s *t, int64_t over_shoot)
{
    #define MIN_TIMER_US 500
    t->start = esp_timer_get_time();
    if ((t->duration - over_shoot) > MIN_TIMER_US)
        t->end = t->start + t->duration - over_shoot;
    else
        t->end = t->start + MIN_TIMER_US;
}

bool timer_over(struct timer_s *t, int64_t *over_shoot)
{
    if (!t->end)
        return true;

    unsigned long us = esp_timer_get_time();
    if (t->end <= us) {
        if (over_shoot)
            *over_shoot = us - t->end;
        t->end = 0;
        return true;
    }
    return false;
}

void timer_start(struct timer_s *t, int64_t duration_us, int (*cb)(int64_t, void*), void * user_data)
{
    t->callback = cb;
    t->duration = duration_us;
    t->user_data = user_data;

    t->start = esp_timer_get_time();
    t->end = t->start + t->duration;
}

int64_t process_timer(struct timer_s *timers, int max_timer)
{
    struct timer_s *t;
    int64_t over_shoot = 0;
    int64_t need_wait = INT64_MAX;

    for (int i = 0; i < max_timer; i++) {
        t = &timers[i];
        if (t->end == 0)
            continue;

        if (timer_over(t, &over_shoot)) {
            if (t->callback)
                switch (t->callback(over_shoot, t->user_data)){
                    case TIMER_RESTART:
                        timer_restart(t, over_shoot);
                        need_wait = min(need_wait, t->end - t->start);
                        break;
                    case TIMER_BREAK_LOOP:
                        return 0;
                    case TIMER_STOP:
                    default:
                        ;; /* Nothing to do */
                }
        } else {
            need_wait = min(need_wait, t->end - esp_timer_get_time());
        }
    }
    return (need_wait == INT64_MAX)? -1 : need_wait;
}

#define MAX_TIMER 8
static struct timer_s timers[MAX_TIMER] = {0};
#define T_RSSI  (&timers[0])
#define T_BLOCK_ENTER  (&timers[1])
#define T_WEBSOCKET  (&timers[2])

int process_rssi(int64_t over_shoot, void *user_data)
{
    ctx_t *ctx = (ctx_t*) user_data;
    lap_counter_t *lc = &ctx->lc;
    config_t *cfg = &ctx->cfg;

    int adc_raw = 0;
    int voltage = 0;

    rx5808_read_rssi(&ctx->rx5808, &adc_raw, &voltage);
//    ESP_LOGI(TAG, "%5d %5dmV", adc_raw, voltage);

    lc->rssi_raw = voltage; //adc_raw;

    float filter = cfg->running.rssi_filter / 100.0f;
    if (filter < 0.01) filter = 0.01;
    lc->rssi_smoothed = (filter * lc->rssi_raw) + ((1.0f - filter) * lc->rssi_smoothed);

    if( lc->in_calib_mode) {
        if (lc->rssi_smoothed > lc->rssi_peak &&
                lc->rssi_smoothed > cfg->running.calib_min_rssi_peak) {
            lc->rssi_peak = lc->rssi_smoothed;
            lc->rssi_enter = lc->rssi_peak * (cfg->eeprom.rssi_offset_enter / 100.0f);
            lc->rssi_leave = lc->rssi_peak * (cfg->eeprom.rssi_offset_leave / 100.0f);
            ESP_LOGI(TAG, "NEW rssi-PEAK: %d enter:%d leave:%d",
                    lc->rssi_peak, lc->rssi_enter, lc->rssi_leave);

            lc->drone_in_gate = false;
            timer_start(T_BLOCK_ENTER, 1000, NULL, NULL);
        }
    }

    if (!lc->rssi_enter || !lc->rssi_leave)
        return TIMER_RESTART;

    if (lc->rssi_enter < lc->rssi_smoothed &&
            timer_over(T_BLOCK_ENTER, NULL) &&
            !lc->drone_in_gate) {
        ESP_LOGI(TAG, "Drone enter gate! rssi: %d", lc->rssi_smoothed);
        timer_start(T_BLOCK_ENTER, 2000, NULL, NULL);
        lc->drone_in_gate = true;
        lc->in_gate_peak_rssi = lc->rssi_smoothed;
        lc->in_gate_peak_millis = sft_millis();

    } else if (lc->drone_in_gate &&
            timer_over(T_BLOCK_ENTER, NULL) &&
            lc->rssi_leave > lc->rssi_smoothed) {
        lc->drone_in_gate = false;
        sft_on_drone_passed(ctx, lc->in_gate_peak_rssi, lc->in_gate_peak_millis);
        timer_start(T_BLOCK_ENTER, 2000, NULL, NULL);

    } else if (lc->drone_in_gate) {
        if (lc->in_gate_peak_rssi < lc->rssi_smoothed) {
            lc->in_gate_peak_rssi = lc->rssi_smoothed;
            lc->in_gate_peak_millis = sft_millis();
        }
    }
    return TIMER_RESTART;
}

int update_websockets(int64_t over_shoot, void *user_data)
{
    ctx_t *ctx = (ctx_t*) user_data;
    lap_counter_t *lc = &ctx->lc;
    static char json_buffer[128];

    static json_writer_t jwmem, *jw;
    jw = &jwmem;
    jw_init(jw, json_buffer, sizeof(json_buffer));

    jw_object(jw){
        jw_kv_int(jw, "t", esp_timer_get_time()/1000);
        jw_kv_int(jw, "r", lc->rssi_raw);
        jw_kv_int(jw, "s", lc->rssi_smoothed);
        jw_kv_str(jw, "i", lc->drone_in_gate ? "true" : "false");
    }

    gui_send_all(ctx, json_buffer);
    return TIMER_RESTART;
}

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
    wifi_init_softap(&ctx.cfg.eeprom);

    captdnsInit();
    ESP_ERROR_CHECK(gui_start(&ctx));

    ESP_ERROR_CHECK(rx5808_init(&ctx.rx5808, PIN_NUM_MOSI,
                                PIN_NUM_CLK, PIN_NUM_CS, PIN_RSSI));
    rx5808_set_channel(&ctx.rx5808, ctx.cfg.eeprom.freq);

    osd_init(&ctx.osd, ctx.cfg.eeprom.elrs_uid);

    /* initialize timers */
    timer_start(T_RSSI, 1000, process_rssi, &ctx);
    timer_start(T_WEBSOCKET, 100000, update_websockets, &ctx);

    sft_update_settings(&ctx);
    cfg_eeprom_to_running(&ctx.cfg);

    int64_t need_wait;
    while (ctx.gui) {

        need_wait = process_timer(timers, MAX_TIMER);
        if (need_wait > 0) {
            vTaskDelay((need_wait / 1000) / portTICK_PERIOD_MS);
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ESP_ERROR_CHECK(gui_stop(&ctx));
}
