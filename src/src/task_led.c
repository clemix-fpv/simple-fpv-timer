#include <stdint.h>
#include <string.h>
#include <simple_fpv_timer.h>
#include <sys/types.h>
#include "esp_timer.h"
#include "config.h"
#include "led.h"
#include "timer.h"

#define LED_GPIO 2

typedef enum {
    LED_MODE_BOOT,
    LED_MODE_SOLID,
    LED_MODE_RACE_START,
    LED_MODE_CALIBRATION,
} led_mode_t;

typedef enum {
    LED_RACE_START_OFFSET_COUNTDOWN,
    LED_RACE_START_COUNTDOWN,
} led_race_start_state_t;

typedef struct {
    led_t led;
    int game_mode;
    int rssi_max;

    led_mode_t mode;

    struct {
        led_race_start_state_t state;
        int countdown;
        millis_t start;
        millis_t offset;
        esp_timer_handle_t timer;
    } race_start;

    struct {
        uint32_t num_leds_orig;
        color_t colors[8];
        uint16_t num_colors;
        uint16_t color_idx;
        millis_t duration;
        esp_timer_handle_t timer;
    } boot;
} task_led_t;

static void task_led_show_rssi(task_led_t *tskled, int rssi)
{
    uint32_t leds = 0;
    int ground = 500;

    if (tskled->rssi_max <= 0)
        return;

    if (rssi > ground) {
        rssi -= ground;
        int max = tskled->rssi_max - ground;

        leds = (tskled->led.num_leds * ((rssi * 100)/ max)) / 100;
    }


    printf("num_leds:%lu leds:%lu rssi:%d max:%d\n",
           tskled->led.num_leds, leds, rssi, tskled->rssi_max);

    led_set(&tskled->led, 0, -1, 0);
    led_set(&tskled->led, 0, leds, COLOR_GREEN);
    led_refresh(&tskled->led);
}

void task_led_on_rssi_update(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    task_led_t *task = (task_led_t*) ctx;
    sft_event_rssi_update_t *ev = (sft_event_rssi_update_t*) event_data;
    int rssi = 0;

    if (task->mode != LED_MODE_CALIBRATION)
        return;

    if (ev->cnt <= 0)
        return;

    for (int i = 0; i < ev->cnt; i++)
        rssi += ev->data[i].rssi;
    rssi /= ev->cnt;

    switch(task->game_mode) {
        case CFG_GAME_MODE_RACE:
            task_led_show_rssi(task, rssi);
        break;
        default:
        /* NOTHING */
        ;
    }

}

static void task_led_on_race_start_timer(void* arg)
{
    task_led_t *task = (task_led_t*) arg;

    millis_t elapsed = (get_millis() - task->race_start.start) / 1000;
    uint32_t num_leds = task->led.num_leds;
    millis_t offset = task->race_start.offset / 1000;
    uint32_t num_leds_countdown = num_leds / 3;

    if (task->mode != LED_MODE_RACE_START)
        return;

    /*printf("state:%d elapsed:%"PRIu64" offset:%"PRIu64" countdown:%d\n",*/
    /*       task->race_start.state, elapsed, offset,*/
    /*       task->race_start.countdown*/
    /*       );*/
    if (task->race_start.state == LED_RACE_START_OFFSET_COUNTDOWN) {
        if (elapsed < offset) {
            uint32_t off_leds = (elapsed * num_leds) / offset;
            printf("off_leds: %"PRIu32"\n", off_leds);

            led_set(&task->led, 0 , off_leds, 0);
            led_refresh(&task->led);
        } else {
            led_set(&task->led, 0 , -1, 0);
            led_refresh(&task->led);
            task->race_start.state = LED_RACE_START_COUNTDOWN;
            task->race_start.countdown = 0;

        }
        esp_timer_start_once(task->race_start.timer, 200 * 1000);

    } else if (task->race_start.countdown < 3){
        task->race_start.countdown++;
        led_set(&task->led, 0 , num_leds_countdown * task->race_start.countdown , COLOR_RED);
        led_refresh(&task->led);
        esp_timer_start_once(task->race_start.timer, 1 * 1000 * 1000);

    } else  if (task->race_start.countdown == 3){
        task->race_start.countdown++;
        led_refresh_all(&task->led, COLOR_GREEN);
        esp_timer_start_once(task->race_start.timer, 10 * 1000 * 1000);

    } else  if (task->race_start.countdown >= 4){
        task->mode = LED_MODE_SOLID;
    }
}

void task_led_on_start_race(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    task_led_t *task = (task_led_t*) ctx;
    sft_event_start_race_t *ev = (sft_event_start_race_t*) event_data;


    task->mode = LED_MODE_RACE_START;
    task->race_start.state = LED_RACE_START_OFFSET_COUNTDOWN;
    task->race_start.offset = ev->offset;
    task->race_start.start = get_millis();

    if (task->race_start.timer)
        esp_timer_stop(task->race_start.timer);

    else {
        const esp_timer_create_args_t timer_args = {
            .callback = &task_led_on_race_start_timer,
            .arg = (void*) task,
            .name = "led-race-start"
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &task->race_start.timer));
    }

    led_refresh_all(&task->led, COLOR_BLUE);

    esp_timer_start_once(task->race_start.timer, 1 * 1000 * 1000);
}


void task_led_on_boot_timer(void *arg)
{
    task_led_t *task = (task_led_t*) arg;

    if (task->mode != LED_MODE_BOOT)
        return;

    if (task->boot.color_idx < task->boot.num_colors) {
        led_refresh_all(&task->led, task->boot.colors[task->boot.color_idx]);
        task->boot.color_idx ++;
        esp_timer_start_once(task->boot.timer, task->boot.duration * 1000);
    } else {

        led_set_num_leds(&task->led, task->boot.num_leds_orig);
    }
}

void task_led_start_boot(task_led_t* task)
{

    task->mode = LED_MODE_BOOT;
    task->boot.num_leds_orig = task->led.num_leds;
    task->boot.colors[0] = COLOR_RED;
    task->boot.colors[1] = COLOR_GREEN;
    task->boot.colors[2] = COLOR_BLUE;
    task->boot.colors[3] = COLOR(252, 186, 3); /* yellow */
    task->boot.colors[4] = COLOR(207, 3, 252); /* purple */
    task->boot.colors[5] = COLOR(3, 252, 252); /* light blue */
    task->boot.colors[6] = COLOR(252, 3, 119); /* pink */
    task->boot.colors[7] = COLOR(252, 102,3 ); /* orange */

    task->boot.num_colors = 8;
    task->boot.color_idx = 0;
    task->boot.duration = 3000;
    if (task->boot.timer) {
        esp_timer_stop(task->boot.timer);
    } else {
        const esp_timer_create_args_t timer_args = {
            .callback = &task_led_on_boot_timer,
            .arg = (void*) task,
            .name = "led-boot"
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &task->boot.timer));
    }
    led_set_num_leds(&task->led, max(task->boot.num_leds_orig, 256));
    esp_timer_start_once(task->boot.timer, 1);
}

void task_led_init(const ctx_t *ctx)
{
    static task_led_t led = {0};
    memset (&led, 0, sizeof(led));

    led.game_mode = ctx->cfg.eeprom.game_mode;
    led.rssi_max = ctx->cfg.eeprom.rssi[0].peak;
    led.race_start.offset = ctx->cfg.eeprom.race_start_offset;

    led_init(&led.led, LED_GPIO, ctx->cfg.eeprom.led_num);



    esp_event_handler_register(SFT_EVENT, SFT_EVENT_RSSI_UPDATE, task_led_on_rssi_update, &led);
    esp_event_handler_register(SFT_EVENT, SFT_EVENT_START_RACE, task_led_on_start_race, &led);

    task_led_start_boot(&led);
}


