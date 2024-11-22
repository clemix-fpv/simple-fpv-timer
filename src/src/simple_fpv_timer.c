// SPDX-License-Identifier: GPL-3.0+

#include "simple_fpv_timer.h"
#include "config.h"
#include "json.h"
#include "esp_log.h"
#include <esp_timer.h>
#include <esp_netif.h>
#include <esp_http_client.h>
#include <esp_netif_ip_addr.h>
#include "lwip/def.h"
#include "osd.h"

ESP_EVENT_DEFINE_BASE(SFT_EVENT);

const char * TAG = "SFT";

void sft_send_new_lap(ctx_t *ctx, lap_t *lap);

bool sft_encode_settings(ctx_t *ctx, json_writer_t *jw)
{
    jw_object(jw) {
        jw_kv(jw, "config") {
            cfg_json_encode(&ctx->cfg.eeprom, jw);
        }
        jw_kv(jw, "status") {
            sft_encode_lapcounter(&ctx->lc, jw);
        }
    }
    return !jw->error;
}

bool sft_lap_encode(lap_t *lap, json_writer_t *jw)
{
    jw_object(jw){
                jw_kv_int(jw, "id", lap->id);
                jw_kv_int(jw, "duration", lap->duration_ms);
                jw_kv_int(jw, "rssi", lap->rssi);
                jw_kv_int(jw, "abs_time", lap->abs_time_ms);
    }
    return !jw->error;
}

bool sft_player_encode(player_t *player, json_writer_t *jw)
{
    struct lap_s *lap;

    jw_object(jw) {
        jw_kv_str(jw, "name", player->name);
        jw_kv_ip4(jw, "ipaddr", player->ip4);
        jw_kv(jw, "laps") {
            jw_array(jw){
                int j = (player->next_idx + MAX_LAPS) % MAX_LAPS;
                for(int i = 0; i < MAX_LAPS ; i++){
                    lap = &player->laps[(j+i) % MAX_LAPS];
                    if (lap->id == 0)
                        continue;
                    if (!sft_lap_encode(lap, jw))
                        return false;
                }
            }
        }
    }
    return !jw->error;
}

bool sft_encode_lapcounter(lap_counter_t *lc, json_writer_t *jw)
{
    struct player_s *player;


    jw_object(jw){
        jw_kv(jw, "in_calib_mode") {
            jw_array(jw) {
                for (int i = 0; i < CFG_MAX_FREQ; i++) {
                    jw_int(jw, lc->in_calib_mode[i]);
                }
            }
        }
        jw_kv(jw, "in_calib_lap_count") {
            jw_array(jw) {
                for (int i = 0; i < CFG_MAX_FREQ; i++) {
                    jw_int(jw, lc->in_calib_lap_count[i]);
                }
            }
        }

        jw_kv(jw, "players") {
            jw_array(jw){
                for (int i = 0; i < MAX_PLAYER; i++) {
                    player = &lc->players[i];
                    if (i > 0 && strlen(player->name) == 0)
                        continue;
                    if (!sft_player_encode(player, jw))
                        return false;
                }
            }
        }
    }

    return !jw->error;
}

struct player_s* sft_player_get_or_create(lap_counter_t *lc, ip4_addr_t ip4, const char *name)
{
    struct player_s *player, *end = &lc->players[MAX_PLAYER-1];

    if (!lc || !ip4.addr)
        return NULL;

    for (player = &lc->players[1]; player != end; player++) {

        if (player->ip4.addr == ip4.addr){
            if (name)
                strncpy(player->name, name, MAX_NAME_LEN);
            return player;
        }
    }

    if (!name)
        return NULL;

    for (player = &lc->players[1]; player != end; player++) {
        if (player->ip4.addr != 0)
            continue;

        memset(player, 0, sizeof(*player));
        strncpy(player->name, name, MAX_NAME_LEN);
        player->name[MAX_NAME_LEN-1] = 0;
        player->ip4 = ip4;
        return player;
    }

    return NULL;
}

struct lap_s* sft_player_add_lap(struct player_s *player,
        int id, int rssi, millis_t duration, millis_t abs_time)
{
    struct lap_s *lap;

    if (!player || !rssi || !duration)
        return NULL;

    lap = &player->laps[player->next_idx % MAX_LAPS];
    player->next_idx++;

    lap->id = id > 0 ? id : player->next_idx;
    lap->rssi = rssi;
    lap->duration_ms = duration;
    lap->abs_time_ms = abs_time;

    return lap;
}

lap_t * sft_player_get_fastes_lap(player_t *p)
{
    lap_t *lap = p->laps;
    lap_t *fastes = NULL;

    for (int i = 0; i < MAX_LAPS; i++, lap++) {
        if (lap->rssi == 0)
            continue;
        if (!fastes)
            fastes = lap;
        else if (lap->duration_ms < fastes->duration_ms)
            fastes = lap;
    }
    return fastes;
}

void sft_on_drone_passed(ctx_t *ctx, int freq, int rssi, millis_t abs_time_ms)
{
    lap_counter_t *lc = &ctx->lc;
    config_t *cfg = &ctx->cfg;
    static millis_t last_lap_time = 0;
    int idx = 0;

    ESP_LOGI(TAG, "Drone passed!");

    for (idx = 0; idx < CFG_MAX_FREQ; idx++) {
        if (cfg->running.rssi[idx].freq == freq)
            break;
    }

    if (idx >= CFG_MAX_FREQ)
        return;

    if (lc->in_calib_mode[idx]) {
        lc->in_calib_lap_count[idx] ++;

        if (cfg_has_elrs_uid(&cfg->eeprom)) {
            char b[64];
            sprintf(b, "calib: %d/%d rssi:%d", lc->in_calib_lap_count[idx],
                    cfg->running.rssi[idx].calib_max_lap_count, rssi);
            osd_display_text(&ctx->osd, cfg->running.osd_x, cfg->running.osd_y, b);
        }

        if (lc->in_calib_lap_count[idx] >= cfg->eeprom.rssi[idx].calib_max_lap_count) {
            lc->in_calib_mode[idx] = false;
            cfg->eeprom.rssi[idx].peak = rssi;
            cfg_save(cfg);
            cfg_eeprom_to_running(cfg);
        }
    } else {
        if (last_lap_time > 0) {

            struct lap_s *lap = sft_player_add_lap(&lc->players[0],
                                               -1, rssi,
                                               abs_time_ms - last_lap_time,
                                               abs_time_ms);

            if (ctx->wifi.state == WIFI_STA) {
                sft_send_new_lap(ctx, lap);
            }
            ESP_LOGI(TAG, "LAP[%d]: %llums rssi:%d", lap->id, lap->duration_ms, lap->rssi);
            if (cfg_has_elrs_uid(&cfg->eeprom)) {

                struct lap_s *fastes = sft_player_get_fastes_lap(&lc->players[3]);
                long fastes_duration = fastes ? fastes->duration_ms : 0;
                fastes = NULL;
                long diff = (long)lap->duration_ms - fastes_duration;
                osd_send_lap(&ctx->osd, lap->id, lap->duration_ms, diff);
            }

        }
        last_lap_time = abs_time_ms;
    }
}

static void dump_pkt(uint8_t *buf, uint8_t len)
{
    uint8_t i;

    printf("Buffer len: %d\n", len);
    for (i=0; i < len; i++) {
        if (i > 0 && (i % 8) == 0) {
            printf("\n");
        }
        printf("%02hhx ", buf[i]);
    }
    printf("\n");
}

bool sft_update_settings(ctx_t *ctx)
{
    lap_counter_t *lc = &ctx->lc;
    config_t *cfg = &ctx->cfg;

    #define cfg_differ_str(cfg, field) \
        (memcmp(cfg->eeprom.field, cfg->running.field, sizeof(cfg->running.field)) != 0)
    #define cfg_set_running_str(cfg, field) \
        memcpy(cfg->running.field, cfg->eeprom.field, sizeof(cfg->running.field))

    #define cfg_differ(cfg, field) \
        (memcmp(&cfg->eeprom.field, &cfg->running.field, sizeof(cfg->running.field)) != 0)
    #define cfg_set_running(cfg, field) \
        memcpy(&cfg->running.field, &cfg->eeprom.field, sizeof(cfg->running.field))


    #define update_rssi(cfg, idx) \
    if (cfg_differ_str(cfg, rssi[idx].name)) {                          \
        snprintf(lc->players[idx].name, sizeof(lc->players[idx].name),    \
             "%s", ctx->cfg.eeprom.rssi[idx].name);                     \
        cfg_set_running_str(cfg, rssi[idx].name);                       \
    }


    if (!cfg_changed(cfg)) {
        ESP_LOGI(TAG, "%s -- nothing to do", __func__);
        return true;
    }

    update_rssi(cfg, 0);

    cfg_set_running_str(cfg, magic);

    /* will be handled in task_rssi() event handler */
    cfg_set_running(cfg, rssi[0].peak);
    cfg_set_running(cfg, rssi[0].filter);
    cfg_set_running(cfg, rssi[0].offset_enter);
    cfg_set_running(cfg, rssi[0].offset_leave);
    cfg_set_running(cfg, rssi[0].calib_max_lap_count);
    cfg_set_running(cfg, rssi[0].calib_min_rssi_peak);
    cfg_set_running(cfg, rssi[0].freq);

    memset(lc->in_calib_mode, 0, CFG_MAX_FREQ * sizeof(bool));
    memset(lc->in_calib_lap_count, 0, CFG_MAX_FREQ * sizeof(int));

    if (cfg_differ(cfg, osd_format)) {
        osd_set_format(&ctx->osd, cfg->eeprom.osd_format);
        cfg_set_running_str(cfg, osd_format);
    }

    if (cfg_differ(cfg, osd_x)) {
        ctx->osd.x = cfg->eeprom.osd_x;
        cfg_set_running(cfg, osd_x);
    }

    if (cfg_differ(cfg, osd_y)) {
        ctx->osd.y = cfg->eeprom.osd_y;
        cfg_set_running(cfg, osd_y);
    }

    if (cfg_differ(cfg, elrs_uid) || cfg_differ(cfg, wifi_mode) ||
        cfg_differ(cfg, passphrase) || cfg_differ(cfg, ssid)) {

        wifi_setup(&ctx->wifi, &ctx->cfg.eeprom);

        cfg_set_running_str(cfg, elrs_uid);
        cfg_set_running(cfg, wifi_mode);
        cfg_set_running_str(cfg, passphrase);
        cfg_set_running_str(cfg, ssid);
    }

    sft_event_cfg_changed_t ev = { .cfg = cfg->running };
    ESP_ERROR_CHECK(
        esp_event_post(SFT_EVENT, SFT_EVENT_CFG_CHANGED,
                       &ev, sizeof(ev), pdMS_TO_TICKS(500)));

    if (cfg_changed(cfg)) {
        ESP_LOGI(TAG, "%s -- ERROR not all settings applied!", __func__);
        cfg_dump(cfg);
        return false;
    }
    return true;
}


/**
 * This function get's called, once the station connects to a AP
 * and a IP address was assigned!
 */
void sft_register_me(ctx_t *ctx, ip4_addr_t *server)
{
    static char url[64];
    static char json[64];
    static json_writer_t jw;
    esp_err_t err;


    jw_init(&jw, json, sizeof(json));

    snprintf(url, sizeof(url), "http://"IPSTR"/api/v1/player/connect", IP2STR(server));
    jw_object(&jw){
        jw_kv_str(&jw, "player", ctx->cfg.eeprom.rssi[0].name);
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    /*esp_http_client_set_url(client, url);*/
    /*esp_http_client_set_method(client, HTTP_METHOD_POST);*/
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
}

/**
 *
 */
void sft_send_new_lap(ctx_t *ctx, lap_t *lap)
{
    static char url[64];
    static char json[64];
    static json_writer_t jw;
    esp_err_t err;

    jw_init(&jw, json, sizeof(json));

    snprintf(url, sizeof(url),
             "http://"IPSTR"/api/v1/player/lap",
             IP2STR(&ctx->server_ipv4));
    jw_object(&jw){
        jw_kv_str(&jw, "player", ctx->cfg.eeprom.rssi[0].name);
        jw_kv_int(&jw, "id", lap->id);
        jw_kv_int(&jw, "rssi", lap->rssi);
        jw_kv_int(&jw, "duration", lap->duration_ms);
    }

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json, strlen(json));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
}

void ip_event_handler(void *ctxp, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ctx_t *ctx = (ctx_t*) ctxp;
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ip = (ip_event_got_ip_t*) event_data;
        ctx->server_ipv4.addr = ip->ip_info.gw.addr;
        ip4_addr_t addr = {.addr = ip->ip_info.gw.addr};

        ESP_LOGI(TAG, "STA got IP gw:" IPSTR, IP2STR(&ip->ip_info.gw));
        sft_register_me(ctx, &addr);

    } else {
        ESP_LOGI(TAG, "Got event: %ld", event_id);
    }
}

void sft_event_handler(void* handler_arg, esp_event_base_t base, int32_t id, void* event_data)
{

}

void sft_event_drone_passed(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    sft_event_drone_passed_t *ev = (sft_event_drone_passed_t*)event_data;
    sft_on_drone_passed(ctx, ev->freq, ev->rssi, ev->abs_time_ms);
}

void sft_init(ctx_t *ctx)
{
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, ctx);
    esp_event_handler_register(SFT_EVENT, SFT_EVENT_DRONE_PASSED, sft_event_drone_passed, ctx);
}



void sft_start_calibration(ctx_t *ctx)
{
    lap_counter_t *lc = &ctx->lc;

    for(int i = 0; i < CFG_MAX_FREQ; i++)
        lc->in_calib_mode[i] = true;
    memset(lc->in_calib_lap_count, 0, CFG_MAX_FREQ * sizeof(int));
}
