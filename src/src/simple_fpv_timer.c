// SPDX-License-Identifier: GPL-3.0+

#include "simple_fpv_timer.h"
#include "config.h"
#include "esp_err.h"
#include "gui.h"
#include "json.h"
#include "esp_log.h"
#include <stdlib.h>
#include <esp_timer.h>
#include <esp_netif.h>
#include <esp_http_client.h>
#include <esp_netif_ip_addr.h>
#include <string.h>
#include "lwip/def.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "osd.h"
#include "timer.h"

ESP_EVENT_DEFINE_BASE(SFT_EVENT);

static const char * TAG = "SFT";

void sft_send_new_lap(ctx_t *ctx, lap_t *lap);
void sft_register_me(ctx_t *ctx);
bool sft_build_api_url(ctx_t *ctx, const char *path, char *buf, int buf_len);
ip4_addr_t get_ip(ctx_t *ctx);

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
                    if (strlen(player->name) == 0)
                        continue;
                    if (!sft_player_encode(player, jw))
                        return false;
                }
            }
        }
    }

    return !jw->error;
}

static void sft_emit_led_static(ctx_t *ctx, color_t color)
{

    sft_led_command_t *cmd = NULL;
    uint16_t sz = sizeof(sft_event_led_command_t) + sizeof(sft_led_command_t);
    sft_event_led_command_t *ev = calloc(1, sz);

    ev->num = 1;
    cmd = &ev->commands[0];
    cmd->type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION;
    cmd->color = color;
    cmd->num = 0;
    cmd->num_end = 100;
    cmd->update_interval = 100;
    cmd->duration = 2000;

    esp_event_post(SFT_EVENT, SFT_EVENT_LED_COMMAND, ev, sz, pdMS_TO_TICKS(500));
    free(ev);
}

void sft_emit_led_blink(ctx_t *ctx, color_t color)
{
    sft_led_command_t *cmd;
    size_t sz = sizeof(sft_event_led_command_t) + sizeof(sft_led_command_t) * 5;
    sft_event_led_command_t *ev = calloc(1, sz);

    ev->num = 5;
    cmd = &ev->commands[0];
    cmd->type = SFT_LED_CMD_TYPE_PERCENT_TRANSITION;
    cmd->color = 0;
    cmd->num = 0;
    cmd->num_end = 100;
    cmd->offset = 100;
    cmd->offset_end = 0;
    cmd->update_interval = 100;
    cmd->duration = 1000;

    cmd = &ev->commands[1];
    cmd->color = color;
    cmd->num = 100;
    cmd->duration = 500;

    cmd = &ev->commands[2];
    cmd->color = 0;
    cmd->num = 100;
    cmd->duration = 300;

    cmd = &ev->commands[3];
    cmd->color = color;
    cmd->num = 100;
    cmd->duration = 500;

    cmd = &ev->commands[4];
    cmd->color = 0;
    cmd->num = 100;
    cmd->duration = 100;

    esp_event_post(SFT_EVENT, SFT_EVENT_LED_COMMAND, ev, sz, pdMS_TO_TICKS(500));
    free(ev);
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

esp_err_t sft_ctf_send_rssi_config(ctx_t *ctx, ip4_addr_t *ip) {

    static const int buf_len = 1024 * 3;
    char *buf;
    static char url[64];
    json_writer_t jw_obj, *jw;
    esp_err_t err = ESP_ERR_TIMEOUT;

    if (!(buf = malloc(buf_len))){
        ESP_LOGE(TAG, "RSSI update - out of memory!");
        return ESP_ERR_NO_MEM;
    }

    jw = &jw_obj;
    jw_init(jw, buf, buf_len);

    #define starts_with(a,b) (strncmp(a, b, strlen(b))  == 0)
    #define ends_with(a,b)  (strncmp(a + (strlen(a) - strlen(b)), b, strlen(b))  == 0)


    snprintf(url, sizeof(url), "http://%s/api/v1/settings", ip4addr_ntoa(ip));
#if 1
    for(int i = 0; i < CFG_MAX_FREQ; i++) {
        const struct config_meta* cm = cfg_meta();
        char prefix[16];
        jw_init(jw, buf, buf_len);
        snprintf(prefix, 16, "rssi[%d]", i);
        jw_object(jw) {
            for(; cm->name != NULL; cm++) {
                if (starts_with(cm->name, prefix)) {
                    if (
                        ends_with(cm->name, "name") ||
                        ends_with(cm->name, "freq") ||
                        ends_with(cm->name, "peak") ||
                        ends_with(cm->name, "filter") ||
                        ends_with(cm->name, "offset_enter") ||
                        ends_with(cm->name, "offset_leave") ||
                        ends_with(cm->name, "led_color")
                    ) {
                        cfg_meta_json_encode(&ctx->cfg.eeprom, cm, jw);
                    }
                }
            }
        }
        if (!jw->error) {
            ESP_LOGI(TAG, "%s", jw->buf);
            gui_send_http(ctx, url, jw->buf);
        } else {
            ESP_LOGE(TAG, "JSON error on sending CTF/RSSI cfg, need %"PRIu16" more", jw->needed_space);
        }
    }
#else
    const struct config_meta* cm = cfg_meta();
    char prefix[16];
    jw_init(jw, buf, buf_len);
    snprintf(prefix, 16, "rssi[");
    jw_object(jw) {
        for(; cm->name != NULL; cm++) {
            if (starts_with(cm->name, prefix)) {
                if (
                    ends_with(cm->name, "name") ||
                    ends_with(cm->name, "freq") ||
                    ends_with(cm->name, "peak") ||
                    ends_with(cm->name, "filter") ||
                    ends_with(cm->name, "offset_enter") ||
                    ends_with(cm->name, "offset_leave") ||
                    ends_with(cm->name, "led_color")
                ) {
                    cfg_meta_json_encode(&ctx->cfg.eeprom, cm, jw);
                }
            }
        }
    }
    if (!jw->error) {
        ESP_LOGI(TAG, "%s", jw->buf);
        gui_send_http(ctx, url, jw->buf);
    } else {
        ESP_LOGE(TAG, "JSON error on sending CTF/RSSI cfg, need %"PRIu16" more", jw->needed_space);
    }
#endif
    free(buf);

    return err;
}

esp_err_t sft_ctf_connect_node(ctx_t *ctx, ip4_addr_t ip, const char *name)
{
    ctf_t *ctf = &ctx->ctf;
    ctf_node_t *node = NULL;

    int i;

    ESP_LOGI(TAG, "CTF connect node: %s(%s)", name, ip4addr_ntoa(&ip));
    for(i=1; i < MAX_PLAYER; i++)
        if (ctf->nodes[i].ipv4.addr == 0)
            break;

    if (i >= MAX_PLAYER)
        return ESP_ERR_NO_MEM;

    node = &ctf->nodes[i];
    memset (node, 0, sizeof(ctf_node_t));

    node->ipv4 = ip;

    return ESP_OK;
}

esp_err_t sft_on_player_connect(ctx_t *ctx, ip4_addr_t ip, const char *name)
{
    switch(ctx->cfg.eeprom.game_mode) {
        case CFG_GAME_MODE_RACE:
            return sft_player_get_or_create(&ctx->lc, ip, name)?
                ESP_OK : ESP_ERR_NO_MEM;
        case CFG_GAME_MODE_CTF:
            return sft_ctf_connect_node(ctx, ip, name);
        default:
        return ESP_ERR_NOT_FOUND;
    }
}

void sft_send_players_update_to_gui(ctx_t *ctx)
{
    static const int buf_len = 1024;
    char *buf;
    json_writer_t jw;
    player_t *player;

    if (!(buf = malloc(buf_len))) {
        ESP_LOGE(TAG, "Out of memory!");
        return;
    }
    jw_init(&jw, buf, buf_len);

    jw_object(&jw){
        jw_kv_str(&jw, "type", "players");
        jw_kv(&jw, "players"){
            jw_array(&jw) {
                for (int i = 0; i < MAX_PLAYER; i++) {
                    player = &ctx->lc.players[i];
                    if (strlen(player->name) == 0)
                        continue;
                    if (!sft_player_encode(player, &jw)) {
                        ESP_LOGE(TAG, "Failed to encode player");
                        goto out;
                    }
                }
            }
        }
    }

    if (!jw.error)
        gui_send_all(ctx, buf);

out:
    free(buf);
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

esp_err_t sft_on_player_lap(ctx_t *ctx, ip4_addr_t ip4, int id, int rssi, millis_t duration) {

    if (sft_player_add_lap(sft_player_get_or_create(&ctx->lc, ip4, NULL),
                                       id, rssi, duration, get_millis())) {
        sft_send_players_update_to_gui(ctx);
        return ESP_OK;
    }

    return ESP_ERR_NO_MEM;
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

void sft_on_drone_passed_race(ctx_t *ctx, int freq, int rssi, millis_t abs_time_ms)
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

            if (ctx->cfg.eeprom.node_mode == CFG_NODE_MODE_CHILD)
                sft_send_new_lap(ctx, lap);
            else
                sft_send_players_update_to_gui(ctx);

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

void ctf_node_set_current(ctf_node_t *node, int team_idx)
{

    ESP_LOGE(TAG, "%s - set %d", __func__, team_idx);
    if (node->current == team_idx)
        return;

    if (node->current >= 0 && node->current < MAX_PLAYER) {
        ctf_team_t *oteam = &node->teams[node->current];
        oteam->captured_ms += get_millis() - oteam->captured_start;
        ESP_LOGE(TAG, "%s - old: %"PRIu64, __func__, oteam->captured_ms);
        oteam->captured_start = 0;
        node->current = -1;
    }
    if (team_idx >= 0 && team_idx < MAX_PLAYER) {
        ctf_team_t *nteam = &node->teams[team_idx];
        node->current = team_idx;
        nteam->captured_start = get_millis();
        ESP_LOGE(TAG, "%s - new: %"PRIu64" enter:%"PRIu64, __func__, nteam->captured_ms, nteam->captured_start);
    }
}

void sft_on_drone_passed_ctf(ctx_t *ctx, int freq, int rssi, millis_t abs_time_ms)
{
    config_t *cfg = &ctx->cfg;
    int idx = 0;
    ctf_team_t *my_team = NULL;
    config_rssi_t *my_rssi = NULL;
    ctf_node_t *node = &ctx->ctf.nodes[0];
    int current_team_idx = -1;

    ESP_LOGI(TAG, "%s - ENTER", __func__);

    int count_drone_in = 0;
    for (idx = 0; idx < CFG_MAX_FREQ; idx++) {
        config_rssi_t *tmp = &cfg->running.rssi[idx];

        if (tmp->freq == 0)
            continue;

        ctf_team_t *team = &node->teams[idx];
        if (tmp->freq == freq) {
            current_team_idx = idx;
            my_team = team;
            my_rssi = tmp;
            team->enter = 0;
        } else {
            if (team->enter > 0 ) {
                count_drone_in++;
            }
        }
    }

    if(my_team && xSemaphoreTake(ctx->sem, ( TickType_t ) 10 ) == pdTRUE ) {
        if (count_drone_in == 0 && my_team) {
            if (node->current != current_team_idx){
                ctf_node_set_current(node, current_team_idx);
                sft_emit_led_static(ctx, my_rssi->led_color);
            }
        } else {
            ctf_node_set_current(node, -1);
            sft_emit_led_blink(ctx, my_rssi->led_color);
        }
        xSemaphoreGive(ctx->sem);
    }
}

void sft_on_drone_passed(ctx_t *ctx, int freq, int rssi, millis_t abs_time_ms)
{
    switch (ctx->cfg.running.game_mode) {
        case CFG_GAME_MODE_RACE:
            sft_on_drone_passed_race(ctx, freq,rssi, abs_time_ms);
            break;
        case CFG_GAME_MODE_CTF:
            sft_on_drone_passed_ctf(ctx, freq,rssi, abs_time_ms);
            break;
        case CFG_GAME_MODE_SPECTRUM:
        default:
            break;
    }
}

void sft_on_drone_enter_ctf(ctx_t *ctx, int freq, int rssi, millis_t abs_time_ms)
{
    config_t *cfg = &ctx->cfg;
    int idx = 0;
    millis_t now = get_millis();
    config_rssi_t *my_rssi = NULL;
    ctf_team_t *my_team = NULL;
    ctf_node_t *node = &ctx->ctf.nodes[0];
    int count_drone_in = 0;
    int current_team_idx = -1;

    ESP_LOGI(TAG, "%s - ENTER", __func__);

    for (idx = 0; idx < CFG_MAX_FREQ; idx++) {
        config_rssi_t *tmp = &cfg->running.rssi[idx];

        if (tmp->freq == 0)
            continue;

        ctf_team_t *team = &node->teams[idx];

        if (tmp->freq == freq) {
            current_team_idx = idx;
            my_team = team;
            my_rssi = tmp;
            team->enter = now;
            count_drone_in ++;
        } else {
            if (team->enter > 0 ) {
                count_drone_in++;
            }
        }
    }

    if(my_team && xSemaphoreTake(ctx->sem, ( TickType_t ) 10 ) == pdTRUE ) {
        if (count_drone_in == 1) {
            if (node->current != current_team_idx){
                ctf_node_set_current(node, current_team_idx);
                sft_emit_led_static(ctx, my_rssi->led_color);
            }
        } else {
            ctf_node_set_current(node, -1);
            sft_emit_led_blink(ctx, my_rssi->led_color);
        }
        xSemaphoreGive(ctx->sem);
    }
}

void sft_on_drone_enter(ctx_t *ctx, int freq, int rssi, millis_t abs_time_ms)
{
    switch (ctx->cfg.running.game_mode) {
        case CFG_GAME_MODE_CTF:
            sft_on_drone_enter_ctf(ctx, freq,rssi, abs_time_ms);
            break;
        case CFG_GAME_MODE_RACE:
        case CFG_GAME_MODE_SPECTRUM:
        default:
            break;
    }
}

static void sft_race_on_1s_timer(void* arg)
{
    ctx_t *ctx = (ctx_t*) arg;
    static int count = 0;
    if (count++ > 10) {
        sft_register_me(ctx);
        count = 0;
    }
}

void sft_race_mode_deinit(ctx_t *ctx)
{
    esp_timer_stop(ctx->race_timer);
    esp_timer_delete(ctx->race_timer);
    memset(&ctx->lc, 0, sizeof(ctx->lc));
}

void sft_race_mode_init(ctx_t *ctx)
{
    memset(&ctx->lc, 0, sizeof(ctx->lc));
    strcpy(ctx->lc.players[0].name, ctx->cfg.running.rssi[0].name);

    const esp_timer_create_args_t timer_args = {
        .callback = &sft_race_on_1s_timer,
        .arg = (void*) ctx,
        .name = "sft-race-timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &ctx->race_timer));
    esp_timer_start_periodic(ctx->race_timer, 1000 * 1000);

}

void sft_ctf_start(ctx_t *ctx, millis_t duration_ms)
{
    int i, j;
    ctf_t *ctf = &ctx->ctf;

    ctf->duration_ms = duration_ms;
    ctf->start_time = get_millis();

    for(i = 0; i < MAX_PLAYER; i++) {
        ctf_node_t *node = &ctf->nodes[i];
        node->current = -1;

        for(j=0; j < MAX_PLAYER; j++) {
            ctf_team_t *team = &node->teams[j];
            team->enter = 0;
            team->captured_ms = 0;
        }
    }

    sft_emit_led_static(ctx, COLOR_WHITE);
}

void sft_ctf_stop(ctx_t *ctx)
{
    ctf_t *ctf = &ctx->ctf;

    ctf->duration_ms = 0;
    ctf->start_time = 0;
    sft_emit_led_static(ctx, COLOR_BLACK);
}

static bool sft_ctf_is_running(ctx_t *ctx)
{
    ctf_t *ctf = &ctx->ctf;

    if (ctf->duration_ms > 0) {
        return (get_millis() - ctf->start_time) < ctf->duration_ms;
    }
    return false;
}

static void sft_ctf_send_status_update(ctx_t *ctx)
{
    static const int buf_len = 1024 * 2;
    char *buf;
    json_writer_t jw_obj, *jw;
    ctf_t *ctf = &ctx->ctf;
    int i,j;

    ESP_LOGI(TAG, "ctf_send_status_update!");

    if (!(buf = malloc(buf_len))) {
        return;
    }

    jw = &jw_obj;
    jw_init(jw, buf, buf_len);

    jw_object(jw) {
        jw_kv_str(jw, "type", "ctf");

        jw_kv(jw, "ctf") {
            jw_object(jw) {

                jw_kv(jw, "team_names") {
                    jw_array(jw) {
                        for(i=0; i < ctf->num_teams; i++) {
                            jw_str(jw, ctf->team_names[i]);
                        }
                    }
                }
                jw_kv(jw, "nodes") {
                    jw_array(jw) {
                        for(i=0; i < MAX_PLAYER; i++) {
                            ctf_node_t *node = &ctx->ctf.nodes[i];
                            if (i > 0 && node->ipv4.addr == 0)
                                continue;

                            jw_object(jw) {
                                jw_kv_ip4(jw, "ipv4", node->ipv4);
                                jw_kv_str(jw, "name", node->name);
                                if (node->current >= 0) {
                                    jw_kv_int(jw, "current", node->current);
                                }

                                jw_kv(jw, "captured_ms") {
                                    jw_array(jw) {
                                        for(j=0; j < ctf->num_teams; j++) {
                                            ctf_team_t *team = &node->teams[j];
                                            millis_t ms = team->captured_ms;

                                            if (node->current == j && team->captured_start > 0)
                                                ms += get_millis() - team->captured_start;
                                            jw_uint64(jw, ms);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    if (!jw->error) {
        if (ctx->cfg.eeprom.node_mode == CFG_NODE_MODE_CHILD) {
            static const int url_len = 64;
            char *url;

            if (!(url = malloc(url_len)))
                goto out;

            if (!sft_build_api_url(ctx, "api/v1/ctf/update", url, url_len)){
                free(url);
                goto out;
            }
            gui_send_http(ctx, url, jw->buf);

            free (url);
        } else {
            gui_send_all(ctx, jw->buf);
        }
    } else {
        ESP_LOGE(TAG, "Buffer to small for sending CTF stats, need %"PRIu16" more", jw->needed_space);
    }

out:
    free(buf);
}

static void sft_ctf_on_1s_timer(void* arg)
{
    ctx_t *ctx = (ctx_t*) arg;
    ctf_t *ctf = &ctx->ctf;
    static int count = 0;

    /*ESP_LOGI(TAG, "On sft_ctf_on_1s_timer!");*/

    if (!sft_ctf_is_running(ctx)){
        if( xSemaphoreTake(ctx->sem, ( TickType_t ) 10 ) == pdTRUE ) {

            ESP_LOGE(TAG, "call node_set_current from %d", __LINE__);
            ctf_node_set_current(&ctf->nodes[0], -1);

            xSemaphoreGive(ctx->sem);
        }
    }

    if (count++ > 3) {
        /* We send the status update also as hardbeat */
        sft_ctf_send_status_update(ctx);
        count = 0;
    }
}

void sft_ctf_mode_deinit(ctx_t *ctx)
{
    esp_timer_stop(ctx->ctf.timer);
    esp_timer_delete(ctx->ctf.timer);
    memset(&ctx->ctf, 0, sizeof(ctx->ctf));
}

void ctf_node_init(ctf_node_t *node) {

    memset(node, 0, sizeof(*node));
    node->current = -1;
}

void sft_ctf_mode_init(ctx_t *ctx)
{
    ctf_t *ctf = &ctx->ctf;
    int i;

    memset(ctf, 0, sizeof(*ctf));

    /* Initializes all nodes */
    for(i=0; i < MAX_PLAYER; i++) {
        ctf_node_t *node = &ctf->nodes[i];
        ctf_node_init(node);
    }

    /* copy team_names out of rssi config */
    for(i=0; i < MAX_PLAYER; i++) {
        config_rssi_t *rssi = &ctx->cfg.running.rssi[i];
        if (rssi->freq > 0)
            strncpy(ctf->team_names[i], rssi->name, MAX_NAME_LEN);
        else
            break;
    }
    ctf->num_teams = i;

    strncpy(ctf->nodes[0].name, ctx->cfg.eeprom.node_name, MAX_NAME_LEN);
    ctf->nodes[0].ipv4 = get_ip(ctx);

    const esp_timer_create_args_t timer_args = {
        .callback = &sft_ctf_on_1s_timer,
        .arg = (void*) ctx,
        .name = "sft-ctf-timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &ctx->ctf.timer));
    esp_timer_start_periodic(ctx->ctf.timer, 1000 * 1000);

    sft_register_me(ctx);
}

void sft_change_game_mode(ctx_t *ctx, uint16_t new_mode, uint16_t old_mode)
{
    switch(old_mode) {
        case CFG_GAME_MODE_RACE:
            sft_race_mode_deinit(ctx);
            break;
        case CFG_GAME_MODE_CTF:
            sft_ctf_mode_deinit(ctx);
            break;
        case CFG_GAME_MODE_SPECTRUM:
        default:
            break;
    }

    switch(new_mode) {
        case CFG_GAME_MODE_RACE:
            sft_race_mode_init(ctx);
            break;
        case CFG_GAME_MODE_CTF:
            sft_ctf_mode_init(ctx);
            break;
        case CFG_GAME_MODE_SPECTRUM:
        default:
            break;
    }
}

void dump_buffer(uint8_t *buf, uint8_t len)
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


    #define update_rssi(cfg, idx, name_changed)                         \
    if (cfg_differ_str(cfg, rssi[idx].name)) {                          \
        name_changed = true;                                            \
        cfg_set_running_str(cfg, rssi[idx].name);                       \
    }                                                                   \
    /* will be handled in task_rssi() event handler */                  \
    cfg_set_running(cfg, rssi[idx].peak);                                 \
    cfg_set_running(cfg, rssi[idx].filter);                               \
    cfg_set_running(cfg, rssi[idx].offset_enter);                         \
    cfg_set_running(cfg, rssi[idx].offset_leave);                         \
    cfg_set_running(cfg, rssi[idx].calib_max_lap_count);                  \
    cfg_set_running(cfg, rssi[idx].calib_min_rssi_peak);                  \
    if (cfg_differ(cfg, rssi[idx].led_color)) {                         \
        name_changed = true;                                            \
        cfg_set_running(cfg, rssi[idx].led_color);                            \
    }                                                                   \
    cfg_set_running(cfg, rssi[idx].freq);


    ESP_LOGI(TAG, "%s -- ENTER", __func__);
    if (!cfg_changed(cfg)) {
        ESP_LOGI(TAG, "%s -- nothing to do", __func__);
        return true;
    }

    bool name_changed = false;
    ESP_LOGI(TAG, "%s:%d ", __func__, __LINE__);
    for (int i = 0; i < MAX_PLAYER; i++) {
        ESP_LOGI(TAG, "%s:%d ", __func__, __LINE__);
        update_rssi(cfg, i, name_changed);
    }

    ESP_LOGI(TAG, "%s:%d ", __func__, __LINE__);
    cfg_set_running_str(cfg, magic);

    cfg_set_running(cfg, led_num);


    ESP_LOGI(TAG, "%s:%d ", __func__, __LINE__);
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

        ESP_LOGI(TAG, "Setup wifi");
        wifi_setup(&ctx->wifi, &ctx->cfg.eeprom);

        cfg_set_running_str(cfg, elrs_uid);
        cfg_set_running(cfg, wifi_mode);
        cfg_set_running_str(cfg, passphrase);
        cfg_set_running_str(cfg, ssid);
    }

    if (cfg_differ(cfg, game_mode) ||
        name_changed ||
        cfg_differ(cfg, node_name) ||
        cfg_differ(cfg, node_mode)
        ) {
        ESP_LOGI(TAG, "Change game mode");
        sft_change_game_mode(ctx, cfg->eeprom.game_mode, cfg->running.game_mode);
        cfg_set_running(cfg, game_mode);
        cfg_set_running(cfg, node_name);
        cfg_set_running(cfg, node_mode);
    }

    sft_event_cfg_changed_t ev = { .cfg = cfg->running };
    ESP_LOGI(TAG, "Emit CFG_CHANGED event");
    if (esp_event_post(SFT_EVENT, SFT_EVENT_CFG_CHANGED,
                       &ev, sizeof(ev), pdMS_TO_TICKS(1000)) != ESP_OK) {
        ESP_LOGE(TAG, "%s FAILED to set CFG_CHANGED event!", __func__);
    }

    if (cfg_changed(cfg)) {
        ESP_LOGI(TAG, "%s -- ERROR not all settings applied!", __func__);
        cfg_dump(cfg);
        return false;
    }
    ESP_LOGI(TAG, "%s DONE", __func__);
    return true;
}


ip4_addr_t get_gw(ctx_t *ctx) {
    esp_netif_ip_info_t ipinfo;
    ip4_addr_t ip = {0};

    esp_netif_t* netif=NULL;
    if (ctx->cfg.eeprom.wifi_mode == CFG_WIFI_MODE_STA) {
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    } else {
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    }

    if (esp_netif_get_ip_info(netif,&ipinfo) == ESP_OK) {
        printf("IP:  " IPSTR "\n", IP2STR(&ipinfo.ip));
        printf("GW:  " IPSTR "\n", IP2STR(&ipinfo.gw));
        printf("MSK: " IPSTR "\n", IP2STR(&ipinfo.netmask));
        ip.addr = ipinfo.gw.addr;
        return ip;
    }

    return ip;
}

ip4_addr_t get_ip(ctx_t *ctx) {
    esp_netif_ip_info_t ipinfo;
    ip4_addr_t ip = {0};

    esp_netif_t* netif=NULL;
    if (ctx->cfg.eeprom.wifi_mode == CFG_WIFI_MODE_STA) {
        netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    } else {
        netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    }

    if (esp_netif_get_ip_info(netif,&ipinfo) == ESP_OK) {
        printf("IP:  " IPSTR "\n", IP2STR(&ipinfo.ip));
        printf("GW:  " IPSTR "\n", IP2STR(&ipinfo.gw));
        printf("MSK: " IPSTR "\n", IP2STR(&ipinfo.netmask));
        ip.addr = ipinfo.ip.addr;
        return ip;
    }

    return ip;
}

bool sft_build_api_url(ctx_t *ctx, const char *path, char *buf, int buf_len) {

    ip4_addr_t ctrl_ip;

    ctrl_ip.addr = ctx->cfg.eeprom.ctrl_ipv4;
    if (ctrl_ip.addr == 0)
        ctrl_ip = get_gw(ctx);

    int len = snprintf(buf, buf_len, "http://%s:%"PRIu16"%s%s",
                       ip4addr_ntoa(&ctrl_ip), ctx->cfg.eeprom.ctrl_port,
                       path[0] == '/' ? "": "/", path);

    return len <= buf_len;
}

/**
 * This function get's called, once the station connects to a AP
 * and a IP address was assigned!
 */
void sft_register_me(ctx_t *ctx)
{
    static const int buf_len = 64;
    char *url = NULL;
    char *json = NULL;
    json_writer_t jw;
    ip4_addr_t local_ip;

    if (ctx->cfg.eeprom.node_mode != CFG_NODE_MODE_CHILD)
        return;

    if (!(url = malloc(buf_len * 2))) {
        ESP_LOGE(TAG, "Out of memory!");
        return;
    }
    json = &url[buf_len];

    if (!sft_build_api_url(ctx, "api/v1/player/connect", url, buf_len)){
        free(url);
        return;
    }


    local_ip = get_ip(ctx);

    jw_init(&jw, json, buf_len);
    if (ctx->cfg.eeprom.game_mode == CFG_GAME_MODE_RACE) {
        jw_object(&jw){
            jw_kv_str(&jw, "player", ctx->cfg.eeprom.rssi[0].name);
            jw_kv_str(&jw, "name", ctx->cfg.eeprom.node_name);
            jw_kv_str(&jw, "ip4", ip4addr_ntoa(&local_ip));
        }
    } else {
        jw_object(&jw){
            jw_kv_str(&jw, "player", ctx->cfg.eeprom.node_name);
            jw_kv_str(&jw, "name", ctx->cfg.eeprom.node_name);
            jw_kv_str(&jw, "ip4", ip4addr_ntoa(&local_ip));
        }
    }

    gui_send_http(ctx, url, jw.buf);
    free(url);
}

/**
 *
 */
void sft_send_new_lap(ctx_t *ctx, lap_t *lap)
{
    static const int buf_len = 64;
    char *buf;
    char *json;
    json_writer_t jw;
    ip4_addr_t local_ip;

    if (!(buf = malloc(buf_len * 2))) {
        ESP_LOGE(TAG, "Out of memory!");
        return;
    }
    json = &buf[buf_len];

    local_ip = get_ip(ctx);
    jw_init(&jw, json, sizeof(json));
    jw_object(&jw){
        jw_kv_str(&jw, "player", ctx->cfg.eeprom.rssi[0].name);
        jw_kv_int(&jw, "id", lap->id);
        jw_kv_int(&jw, "rssi", lap->rssi);
        jw_kv_int(&jw, "duration", lap->duration_ms);
        jw_kv_str(&jw, "ipv4", ip4addr_ntoa(&local_ip));
    }

    if (!sft_build_api_url(ctx, "api/v1/player/lap", buf, buf_len))
        goto out;

    gui_send_http(ctx, buf, json);

out:
    free(buf);
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

        sft_register_me(ctx);
    } else {
        ESP_LOGI(TAG, "Got event: %ld", event_id);
    }
}

void sft_event_drone_passed(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    sft_event_drone_passed_t *ev = (sft_event_drone_passed_t*)event_data;
    sft_on_drone_passed(ctx, ev->freq, ev->rssi, ev->abs_time_ms);
}

void sft_event_drone_enter(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    sft_event_drone_enter_t *ev = (sft_event_drone_passed_t*)event_data;
    sft_on_drone_enter(ctx, ev->freq, ev->rssi, ev->abs_time_ms);
}


void sft_init(ctx_t *ctx)
{
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler, ctx);
    esp_event_handler_register(SFT_EVENT, SFT_EVENT_DRONE_PASSED, sft_event_drone_passed, ctx);
    esp_event_handler_register(SFT_EVENT, SFT_EVENT_DRONE_ENTER, sft_event_drone_enter, ctx);

}



void sft_start_calibration(ctx_t *ctx)
{
    lap_counter_t *lc = &ctx->lc;

    for(int i = 0; i < CFG_MAX_FREQ; i++)
        lc->in_calib_mode[i] = true;
    memset(lc->in_calib_lap_count, 0, CFG_MAX_FREQ * sizeof(int));
}
