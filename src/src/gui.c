// SPDX-License-Identifier: GPL-3.0+

#include <freertos/FreeRTOS.h>
#include <stdarg.h>
#include <sys/param.h>
#include <esp_http_server.h>
#include <esp_log.h>

#include "config.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "jsmn.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "static_files.h"
#include "json.h"
#include "osd.h"
#include "simple_fpv_timer.h"
#include "timer.h"
#include "gui.h"

static const char * TAG = "http";
static const char * OUT_OF_MEMORY = "Out of memory";

typedef struct {
    bool will_rssi_update;
} session_ctx_t;

void session_ctx_free(void *s)
{
    free(s);
}

static esp_err_t get_root_handler(httpd_req_t *req)
{
    ctx_t *ctx = (ctx_t*) req->user_ctx;
    char buf[64];
    char host[32];
    char addr_buf[16];
    esp_netif_ip_info_t ip_info;

    if (httpd_req_get_hdr_value_str(req, "Host", host, sizeof(host)) != ESP_OK) {
        host[0] = 0;
    }

    if (ctx->wifi.state == WIFI_AP)
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);
    else
        esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip_info);

    esp_ip4addr_ntoa(&ip_info.ip, addr_buf, sizeof(addr_buf));

    /* say 404 if it was already redirected */
    if (strcmp(host, addr_buf) == 0 && strcmp(req->uri, "/") != 0) {
        ESP_LOGI(TAG, "404 Not found %s", req->uri);
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_sendstr(req, "");
        return ESP_OK;
    }

    snprintf(buf, sizeof(buf), "http://%s/index.html", addr_buf);
    ESP_LOGI(TAG, "Redirect host:%s uri:%s -> %s", host, req->uri, buf);
    httpd_resp_set_hdr(req, "Location", buf);
    httpd_resp_set_hdr(req, "Connection", "close");
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_sendstr(req, "");

    return ESP_OK;
}

static esp_err_t get_static_handler(httpd_req_t *req)
{
    /* Send a simple response */
    const struct static_files *sf = (const struct static_files *) req->user_ctx;

    httpd_resp_set_type(req, sf->type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "-1");
    httpd_resp_set_hdr(req, "Connection", "close");
    if (sf->is_gzip)
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");

    httpd_resp_send(req, (const char*) sf->data, sf->data_len);

    return ESP_OK;
}

static esp_err_t ws_rssi_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "ENTER: %s", __func__);
    static char ws_buffer[512];

    if (! req->sess_ctx) {
        req->sess_ctx = malloc(sizeof(session_ctx_t));
        req->free_ctx = session_ctx_free;
    } else {
        ESP_LOGI(TAG, "session: %p", req->sess_ctx);
    }

    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        httpd_ws_client_info_t client_info = httpd_ws_get_fd_info(req->handle, fd);

        ESP_LOGI(TAG, "Handshake done, the new connection was opened socket: %d => client info: %d", fd, client_info);
        return ESP_OK;
    }

    int fd = httpd_req_to_sockfd(req);
    ESP_LOGI(TAG, "WS::RCV socket: %d", fd);
    /* Handle received data from websocket */
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*) ws_buffer;
    if (httpd_ws_recv_frame(req, &ws_pkt, sizeof(ws_buffer)) != ESP_OK) {
        ESP_LOGI(TAG, "FAILED to recv ws pkt!");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Received ws pkt length:%d %.*s", ws_pkt.len, ws_pkt.len, ws_buffer);
    return ESP_OK;
}

static void request_send_json(httpd_req_t *req, const char *json, size_t len)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");
    httpd_resp_set_hdr(req, "Expires", "-1");
    httpd_resp_send(req, json, len);
}

static void request_send_ok(httpd_req_t *req)
{
    httpd_resp_set_status(req, "200 OK");
    request_send_json(req, "{\"status\":\"ok\"}", 15);
}

static void request_send_error(httpd_req_t *req, const char *msg, ...)
{
    int len;
    json_writer_t jw;
    va_list args;
    static char err_buf[64];
    static char json_buf[128];

    va_start (args, msg);
    len = vsnprintf (err_buf, sizeof(err_buf), msg, args);
    va_end (args);

    if (len < sizeof(err_buf) && len >= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        jw_init(&jw , json_buf, sizeof(json_buf));
        jw_object(&jw){
            jw_kv_str(&jw, "status", "failed");
            jw_kv_str(&jw, "msg", err_buf);
        }
        request_send_json(req, jw.buf, jw.wptr - jw.buf);

    } else {
        httpd_resp_set_status(req, "500 Internal Server Error");
        request_send_json(req, "{\"status\":\"failed\"}", 19);
    }
}

static esp_err_t api_v1_get_handler(httpd_req_t *req)
{
    ctx_t *ctx = (ctx_t*) req->user_ctx;
    static const int buf_sz = 1024 * 4;
    json_writer_t jw;
    char *buf = NULL;

    if (!(buf = malloc(buf_sz))){
        request_send_error(req, OUT_OF_MEMORY);
        return ESP_ERR_NO_MEM;
    }

    jw_init(&jw, buf, buf_sz);

    ESP_LOGI(TAG, "%s URI: %s", __func__, req->uri);
    if (strcmp(req->uri, "/api/v1/settings") == 0) {
        if (sft_encode_settings(ctx, &jw))
            request_send_json(req, jw.buf, strlen(jw.buf));
        else
            request_send_error(req, "JSON buffer to small - needed:%d", jw.needed_space);

    } else if (strcmp(req->uri, "/api/v1/ctf/stop") == 0) {
        sft_ctf_stop(ctx);
        request_send_ok(req);

    } else {
        request_send_error(req, "Uri %s not found", req->uri);
    }

    free(buf);
    return ESP_OK;
}

static esp_err_t get_remote_ip4(httpd_req_t *req, ip4_addr_t *ip4)
{
    struct sockaddr_in6 addr_in;
    int s = httpd_req_to_sockfd(req);
    socklen_t addrlen = sizeof(addr_in);

    if (lwip_getpeername(s, (struct sockaddr *)&addr_in, &addrlen) != -1) {
        ESP_LOGI(TAG, "Remote IP is %s", inet_ntoa(addr_in.sin6_addr.un.u32_addr[3]));
        ip4->addr = addr_in.sin6_addr.un.u32_addr[3];
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Error getting peer's IP/port");
        return ESP_FAIL;
    }
}

static const char* strstartwith(const char *str, const char *needle)
{
    if (!str || !needle || !strlen(needle))
        return NULL;

    if (strncmp(str, needle, strlen(needle)) == 0)
        return str + strlen(needle);

    return NULL;
}

static inline bool streq(const char *str1, const char *str2)
{
    return strcmp(str1, str2) == 0;
}

static esp_err_t api_v1_post_osd(httpd_req_t *req, ctx_t *ctx, const char *uri_tok, json_t *jr)
{
    osd_t *osd = &ctx->osd;
    json_writer_t jw;
    char *tmp_buf64 = NULL;
    char *tmp2_buf64 = NULL;
    static const int jsmn_tokens_num = 512;

    jsmntok_t *tokens;
    int x, y;


    if (!(tokens = malloc(jsmn_tokens_num))) {
        request_send_error(req, OUT_OF_MEMORY);
        return ESP_ERR_NO_MEM;
    }

    if (!(tmp_buf64 = malloc(64))) {
        free(tokens);
        request_send_error(req, OUT_OF_MEMORY);
        return ESP_ERR_NO_MEM;
    }

    if (!(tmp2_buf64 = malloc(64))) {
        free(tokens);
        free(tmp_buf64);
        request_send_error(req, OUT_OF_MEMORY);
        return ESP_ERR_NO_MEM;
    }

    j_init(jr, tokens, jsmn_tokens_num);

    if (streq(uri_tok, "display_text") || streq(uri_tok, "set_text")) {

        if (j_find_str(jr, "text", tmp_buf64, sizeof(tmp_buf64))
            && j_find_int(jr, "x", &x)
            && j_find_int(jr, "y", &y)){

            if (streq(uri_tok, "display_text"))
                osd_display_text(osd, x, y, tmp_buf64);
            else
                osd_send_text(osd, x, y, tmp_buf64);

            request_send_ok(req);
        } else {
            request_send_error(req, "Missing mandatory json field");
        }

    } else if (streq(uri_tok, "clear")){
        osd_send_clear(osd);
        request_send_ok(req);

    } else if (streq(uri_tok, "display")){
        osd_send_display(osd);
        request_send_ok(req);

    } else if (streq(uri_tok, "test_format")) {
        if (j_find_str(jr, "format", tmp_buf64, sizeof(tmp_buf64))) {
            if (osd_eval_format(osd, tmp_buf64, 23, 1337,666, tmp2_buf64, sizeof(tmp2_buf64))) {
                jw_init(&jw, tmp_buf64, 64);
                jw_object(&jw){
                    jw_kv_str(&jw, "status", "ok");
                    jw_kv_str(&jw, "msg", tmp2_buf64);
                }
                httpd_resp_set_status(req, "200 OK");
                if (!jw.error)
                    request_send_json(req, jw.buf, strlen(jw.buf));
                else
                    request_send_error(req, "Failed to build JSON");
            } else {
                request_send_error(req, "Invalid OSD message format");
            }
        } else {
            request_send_error(req, "Missing mandatory json field");
        }
    }

    free(tokens);
    free(tmp_buf64);
    free(tmp2_buf64);
    return  ESP_OK;
}

static esp_err_t api_v1_post_time_sync(httpd_req_t *req, ctx_t *ctx, json_t *jr)
{
    json_t client;
    json_t server;
    uint64_t val;
    json_writer_t jw;
    static const int buf_w_sz = 512;
    char *buf_w;

    if (!(buf_w = malloc(buf_w_sz))) {
        request_send_error(req, OUT_OF_MEMORY);
        return ESP_ERR_NO_MEM;
    }

    j_find(jr, "server", &server);
    j_find(jr, "client", &client);

    jw_init(&jw, buf_w, buf_w_sz);
    jw_object(&jw) {
        jw_kv(&jw, "server"){
            jw_array(&jw){
                json_t e = {0};
                while(j_next(&server, &e)) {
                    if (j_get_uint64(&e, &val))
                        jw_uint64(&jw, val);
                }
                jw_uint64(&jw, get_millis());
            }
        }
        jw_kv(&jw, "client"){
            jw_array(&jw){
                json_t e = {0};
                while(j_next(&client, &e)) {
                    if (j_get_uint64(&e, &val))
                        jw_uint64(&jw, val);
                }
            }
        }
    }
    if (jw.error) {
        request_send_error(req, "Failed to write json");
    } else {
        httpd_resp_set_status(req, "200 OK");
        request_send_json(req, jw.buf, jw.wptr - jw.buf);
    }

    free(buf_w);
    return ESP_OK;
}


static esp_err_t api_v1_post_handler(httpd_req_t *req)
{
    ctx_t *ctx = (ctx_t*) req->user_ctx;
    lap_counter_t *lc = &ctx->lc;
    static const int tmp_str_sz = 32;
    char *key;
    char *value;
    char *json_buf;
    static const int jsmn_tokens_sz = 512;
    jsmntok_t *jsmn_tokens;
    json_t jr;
    esp_err_t err = ESP_OK;
    const char *tok;

    int json_buffer_sz = (req->content_len > 1024) ? req->content_len : 1024;
    json_buffer_sz = (((json_buffer_sz + 31) / 32) * 32);
    int sz = tmp_str_sz * 2 + jsmn_tokens_sz * sizeof(jsmntok_t) + req->content_len;
    if (!(json_buf = malloc(sz))){
        request_send_error(req, "413 Payload Too Large (%d)", sz);
        return ESP_OK;
    }

    key = &json_buf[json_buffer_sz];
    value = &json_buf[json_buffer_sz + tmp_str_sz];
    jsmn_tokens = (jsmntok_t*) &json_buf[json_buffer_sz + tmp_str_sz * 2];


    int len = httpd_req_recv(req, json_buf, json_buffer_sz);
    if (len > json_buffer_sz) {
        request_send_error(req, "413 Payload Too Large (%d)", len);
        return ESP_OK;
    } else if (len < 0) {
        request_send_error(req, "Failed to read payload");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "%s:%d URI: %s data(%d): %.*s", __func__, __LINE__, req->uri, len, len, json_buf);

    j_init(&jr, jsmn_tokens, jsmn_tokens_sz);
    if (len > 0) {
        if(!j_parse(&jr, json_buf, len)) {
            ESP_LOGE(TAG, "Failed to parse json (len:%d)", len);
            request_send_error(req, "Failed to parse json");
            err = ESP_ERR_INVALID_ARG;
            goto out;
        }
    } else {
        j_parse(&jr, "{}", 2);
    }

    if (strcmp(req->uri, "/api/v1/settings") == 0) {
        json_t e = {0};
        while(j_next(&jr, &e)) {
            if (j_get_kv(&e, key, tmp_str_sz, value, tmp_str_sz)) {
                if (cfg_set_param(&ctx->cfg, key, value) != ESP_OK) {
                    request_send_error(req,"Invalid key/value %s=%s", key, value);
                    goto out;
                }
            }
        }
        cfg_verify(&ctx->cfg);
        if (cfg_save(&ctx->cfg) == ESP_OK) {
            if (sft_update_settings(ctx))
                request_send_ok(req);
            else {
                request_send_error(req, "Failed to apply all settings - reboot");
            }
        } else  {
            request_send_error(req, "Failed to write config to eeprom");
        }

    } else if (strcmp(req->uri, "/api/v1/start_calibration") == 0) {
        sft_start_calibration(ctx);
        request_send_ok(req);

    } else if (strcmp(req->uri, "/api/v1/clear_laps") == 0) {
        struct player_s *player;
        millis_t offset;

        if (!j_find_uint64(&jr, "offset", &offset))
            offset = 30000;

        for (int i=0; i < MAX_PLAYER; i++) {
            player = &lc->players[i];
            memset(player->laps, 0, sizeof(player->laps));
            player->next_idx = 0;
        }

        sft_event_start_race_t ev = {.offset = offset };
        ESP_ERROR_CHECK(
            esp_event_post(SFT_EVENT, SFT_EVENT_START_RACE,
                           &ev, sizeof(ev), pdMS_TO_TICKS(500)));

        request_send_ok(req);

    } else if (strcmp(req->uri, "/api/v1/player/connect") == 0) {
        if (j_find_str(&jr, "player", value, tmp_str_sz)) {
            ip4_addr_t ip4 = {0};
            if (get_remote_ip4(req, &ip4) == ESP_OK) {
                if (sft_on_player_connect(ctx, ip4, value) == ESP_OK)
                    request_send_ok(req);
                else
                    request_send_error(req, "Failed to create player: %s", value);
            } else
            request_send_error(req, "No remote IP");
        } else
        request_send_error(req, "Missing key 'player'");

    } else if (strcmp(req->uri, "/api/v1/player/lap") == 0) {
        json_t lap;
        int id, rssi;
        millis_t duration;
        ip4_addr_t ip4 = {0};

        if (get_remote_ip4(req, &ip4) == ESP_OK &&
            j_find_str(&jr, "player", value, tmp_str_sz) &&
            j_find(&jr, "lap", &lap) &&
            j_find_int(&lap, "id", &id) &&
            j_find_int(&lap, "rssi", &rssi) &&
            j_find_uint64(&lap, "duration", &duration)
        ) {
            if (sft_on_player_lap(ctx, ip4, id, rssi, duration) == ESP_OK)
                request_send_ok(req);
            else
                request_send_error(req, "Failed to add players lap");

        } else
        request_send_error(req, "Failed to parse json");

    } else if (strcmp(req->uri, "/api/v1/rssi/update") == 0) {
        int enabled = 0;
        if (j_find_int(&jr, "enabled", &enabled)) {
            ctx->send_rssi_updates = !! enabled;
            request_send_ok(req);
        } else {
            request_send_error(req, "Failed to parse json");
        }

    } else if ((tok = strstartwith(req->uri, "/api/v1/osd/"))) {
        err = api_v1_post_osd(req, ctx, tok, &jr);

    } else if (strcmp(req->uri, "/api/v1/time-sync") == 0) {
        err = api_v1_post_time_sync(req, ctx, &jr);

    } else if (strcmp(req->uri, "/api/v1/ctf/start") == 0) {
        millis_t duration_ms = 0;
        if (j_find_uint64(&jr, "duration_ms", &duration_ms)) {
            sft_ctf_start(ctx, duration_ms);
            request_send_ok(req);
        } else {
            request_send_error(req, "Failed to parse json");
        }

    } else {
        request_send_error(req, "404 Not found - %s", req->uri);
    }

out:
    free(json_buf);
    return err;
}

void sft_event_rssi_update(void* arg, esp_event_base_t base, int32_t id, void* event_data)
{
    sft_event_rssi_update_t *ev = (sft_event_rssi_update_t*) event_data;
    ctx_t *ctx = (ctx_t*) arg;

    /* do not use static send buffer (e.g. json_buffer) here, as it happens that events get triggered
     * on other syscalls like send(), thus the static buffer might be already dirty!
     */
    char *buf;
    json_writer_t *jw;

    if (!ctx->send_rssi_updates) {
        return;
    }

    if (!(buf = malloc(512))) {
        ESP_LOGE(TAG, "RSSI update - out of memory!");
        return;
    }

    jw = (json_writer_t*) buf;
    jw_init(jw, buf + sizeof(json_writer_t), 512 - sizeof(json_writer_t));

    jw_object(jw){
        jw_kv_str(jw, "type", "rssi");
        jw_kv_int(jw, "freq", ev->freq);
        jw_kv(jw, "data"){
            jw_array(jw) {
                for (int i = 0; i < ev->cnt; i++) {
                    jw_object(jw) {
                        jw_kv_int(jw, "t", ev->data[i].abs_time_ms);
                        jw_kv_int(jw, "s", ev->data[i].rssi);
                        jw_kv_int(jw, "r", ev->data[i].rssi_raw);
                        jw_kv_int(jw, "i", ev->data[i].drone_in_gate);
                    }
                }
            }
        }
    }

    gui_send_all(ctx, jw->buf);
    free(buf);
}

/* Function for starting the webserver */
esp_err_t gui_start(ctx_t *ctx)
{
    const struct static_files *sf;
    esp_err_t err;

    const httpd_uri_t ws = {
        .uri        = "/ws/rssi",
        .method     = HTTP_GET,
        .handler    = ws_rssi_handler,
        .user_ctx   = NULL,
        .is_websocket = true
    };

    const httpd_uri_t api_get = {
        .uri        = "/api/v1/*",
        .method     = HTTP_GET,
        .handler    = api_v1_get_handler,
        .user_ctx   = ctx,
        .is_websocket = true
    };

    const httpd_uri_t api_post = {
        .uri        = "/api/v1/*",
        .method     = HTTP_POST,
        .handler    = api_v1_post_handler,
        .user_ctx   = ctx,
        .is_websocket = true
    };

    /* Generate default configuration */
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 0;
    config.core_id = 0;
    for(sf=STATIC_FILES; sf->name; sf++, config.max_uri_handlers++);
    config.max_uri_handlers++; // / -> /index.html
    config.max_uri_handlers++; // /ws/rssi
    config.max_uri_handlers++; // /api/v1 GET
    config.max_uri_handlers++; // /api/v1 POST
    config.max_open_sockets = 7;

//    config.enable_so_linger = true;

    /* Start the httpd server */
    if ((err = httpd_start(&ctx->gui, &config)) != ESP_OK)
        return err;

    for(sf=STATIC_FILES; sf->name; sf++) {
        httpd_uri_t uri_handler = {
            .uri      = sf->name,
            .method   = HTTP_GET,
            .handler  = get_static_handler,
            .user_ctx = (void*) sf
        };
        httpd_register_uri_handler(ctx->gui, &uri_handler);
    }


    httpd_register_uri_handler(ctx->gui, &ws);
    httpd_register_uri_handler(ctx->gui, &api_post);
    httpd_register_uri_handler(ctx->gui, &api_get);

    httpd_uri_t uri_handler = {
        .uri      = "*",
        .method   = HTTP_GET,
        .handler  = get_root_handler,
        .user_ctx = (void*) ctx
    };
    httpd_register_uri_handler(ctx->gui, &uri_handler);

    esp_event_handler_register(SFT_EVENT, SFT_EVENT_RSSI_UPDATE, sft_event_rssi_update, ctx);
    return ESP_OK;
}

/* Function for stopping the webserver */
esp_err_t gui_stop(ctx_t *ctx)
{
    if (ctx->gui) {
        /* Stop the httpd server */
        httpd_stop(ctx->gui);
    }
    return ESP_OK;
}


esp_err_t gui_send_all(ctx_t *ctx, const char *msg)
{
    static const size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int client_fds[max_clients];

    memset(client_fds, 0, sizeof(int) * max_clients);

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t*) msg;
    ws_pkt.len = strlen(msg);

    esp_err_t ret = httpd_get_client_list(ctx->gui, &fds, client_fds);
    if (ret != ESP_OK) {
        ESP_LOGI(TAG, "invalid size!");
        return ESP_ERR_INVALID_SIZE;
    }

    for (int i = 0; i < fds; i++) {
        /*void *ptr = httpd_sess_get_ctx(ctx->gui, client_fds[i]);*/
        httpd_ws_client_info_t client_info = httpd_ws_get_fd_info(ctx->gui, client_fds[i]);
        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(ctx->gui, client_fds[i], &ws_pkt);
        }
    }
    return ESP_OK;
}

esp_err_t gui_send_http2(ctx_t *ctx, const char *url, const char *json)
{
    esp_err_t err;
    char buf[8];

    snprintf(buf, sizeof(buf), "%"PRId16, strlen(json));
    ESP_LOGI(TAG, "send_http(%s, %s)", url, buf);


    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        //.buffer_size_tx = 1024 * 4,
        //.buffer_size = 1024 * 4,

    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);

    /*esp_http_client_set_url(client, url);*/
    /*esp_http_client_set_method(client, HTTP_METHOD_POST);*/
    esp_http_client_set_header(client, "Accept", "*/*");
    esp_http_client_set_header(client, "Content-Type", "application/json; charset=utf-8");
    /*esp_http_client_set_header(client, "Connection", "Keep-Alive");*/
    /*esp_http_client_set_header(client, "Keep-Alive", "timeout=5, max=200");*/
    esp_http_client_set_header(client, "Content-Length", buf);
    esp_http_client_set_post_field(client, json, strlen(json));
    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
    return err;
}


esp_err_t gui_send_http(ctx_t *ctx, const char *url, const char *post_data) {

    // Declare local_response_buffer with size (MAX_HTTP_OUTPUT_BUFFER + 1) to prevent out of bound access when
    // it is used by functions like strlen(). The buffer should only be used upto size MAX_HTTP_OUTPUT_BUFFER
#define MAX_HTTP_OUTPUT_BUFFER 512
    static char output_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};   // Buffer to store response of http request
    int content_length = 0;
    esp_err_t err;

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    ESP_LOGI(TAG, "URL: %s -(%d) %s", url, strlen(post_data), post_data);
    // GET Request
    esp_http_client_set_url(client, url);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    err = esp_http_client_open(client, strlen(post_data));

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
    } else {
        int wlen = esp_http_client_write(client, post_data, strlen(post_data));
        if (wlen < 0) {
            ESP_LOGE(TAG, "HTTP client failed Write failed %d", wlen);
        }

        content_length = esp_http_client_fetch_headers(client);
        if (content_length < 0) {
            ESP_LOGE(TAG, "HTTP client fetch headers failed");
        } else {
            int data_read = esp_http_client_read_response(client, output_buffer, MAX_HTTP_OUTPUT_BUFFER);
            if (data_read >= 0) {
                uint64_t len = esp_http_client_get_content_length(client);
                ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client), len);
                ESP_LOG_BUFFER_HEX(TAG, output_buffer, strlen(output_buffer));
                int leni = len;
                ESP_LOGI(TAG, "%.*s", leni, output_buffer);
            } else {
                ESP_LOGE(TAG, "Failed to read response");
            }
        }
    }
    esp_http_client_cleanup(client);
    return err;
}
