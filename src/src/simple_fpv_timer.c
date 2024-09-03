#include "simple_fpv_timer.h"
#include "config.h"
#include "json.h"
#include "esp_log.h"
#include <esp_timer.h>
#include <esp_netif.h>
#include "osd.h"

const char * TAG = "SFT";

sft_millis_t sft_millis()
{
    return esp_timer_get_time() / 1000;
}

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
        jw_kv_int(jw, "rssi_smoothed", lc->rssi_smoothed);
        jw_kv_int(jw, "rssi_raw", lc->rssi_raw);
        jw_kv_int(jw, "rssi_peak", lc->rssi_peak);
        jw_kv_int(jw, "rssi_enter", lc->rssi_enter);
        jw_kv_int(jw, "rssi_leave", lc->rssi_leave);
        jw_kv_bool(jw, "drone_in_gate", lc->drone_in_gate);
        jw_kv_bool(jw, "in_calib_mode", lc->in_calib_mode);
        jw_kv_int(jw, "in_calib_lap_count", lc->in_calib_lap_count);

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
        int id, int rssi, sft_millis_t duration, sft_millis_t abs_time)
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

void sft_on_drone_passed(ctx_t *ctx, int rssi, sft_millis_t abs_time_ms)
{
    lap_counter_t *lc = &ctx->lc;
    config_t *cfg = &ctx->cfg;
    static sft_millis_t last_lap_time = 0;

    ESP_LOGI(TAG, "Drone passed!");

    if (lc->in_calib_mode) {
        lc->in_calib_lap_count ++;

        if (cfg_has_elrs_uid(&cfg->eeprom)) {
            char b[64];
            sprintf(b, "calib: %d/%d rssi:%d", lc->in_calib_lap_count,
                    cfg->running.calib_max_lap_count, lc->rssi_peak);
            osd_display_text(&ctx->osd, cfg->running.osd_x, cfg->running.osd_y, b);
        }

        if (lc->in_calib_lap_count >= cfg->eeprom.calib_max_lap_count) {
            lc->in_calib_mode = false;
            cfg->eeprom.rssi_peak = lc->rssi_peak;
            cfg_save(cfg);
            cfg_eeprom_to_running(cfg);
        }
    } else {
        if (last_lap_time > 0) {

            struct lap_s *lap = sft_player_add_lap(&lc->players[0],
                                               -1, rssi,
                                               abs_time_ms - last_lap_time,
                                               abs_time_ms);

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

    if (!cfg_changed(cfg)) {
        ESP_LOGI(TAG, "%s -- nothing to do", __func__);
        return true;
    }

    if (cfg_differ_str(cfg, player_name)) {
        snprintf(lc->players[0].name, sizeof(lc->players[0].name),
             "%s", ctx->cfg.eeprom.player_name);
        cfg_set_running_str(cfg, player_name);
    }

    if (cfg_differ(cfg, freq)) {
        rx5808_set_channel(&ctx->rx5808, cfg->eeprom.freq);
        cfg_set_running(cfg, freq);
    }

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

    if (cfg_changed(cfg)) {
        ESP_LOGI(TAG, "%s -- ERROR not all settings applied!", __func__);
        cfg_dump(cfg);
        return false;
    }
    return true;
}
