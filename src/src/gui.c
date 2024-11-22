// SPDX-License-Identifier: GPL-3.0+

#include <freertos/FreeRTOS.h>
#include <stdarg.h>
#include <sys/param.h>
#include <esp_http_server.h>
#include <esp_log.h>

#include "config.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_netif_types.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "static_files.h"
#include "json.h"
#include "osd.h"
#include "simple_fpv_timer.h"
#include "timer.h"
#include "gui.h"

static const char * TAG = "http";
static char json_buffer[1024*4];
static json_t json_parser;
static jsmntok_t jsmn_tokens[128];
static char tmp_buf64[64];
static char tmp2_buf64[64];

typedef struct {
    bool will_rssi_update;
} session_ctx_t;

void session_ctx_free(void *s)
{
    free(s);
}

/* This function is NOT thread safe and use the
 * static json_buffer to parse.
 * The parameter length give the max string length
 * in json_buffer. If < 0 is given, then strlen()
 * will be used.
 *
 * USE THIS FUNCTION WITH CARE!!!!!
 * You can only parse one JSON object at a time!
 */
static json_t* json_parse_static_buffer(int length)
{
    j_init(&json_parser, jsmn_tokens, 128);

    json_buffer[sizeof(json_buffer) -1 ] = 0;
    if (length < 0) {
        length = strlen(json_buffer);
    }

    return j_parse(&json_parser, json_buffer, length);
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
    ws_pkt.payload = (uint8_t*) json_buffer;
    if (httpd_ws_recv_frame(req, &ws_pkt, sizeof(json_buffer)) != ESP_OK) {
        ESP_LOGI(TAG, "FAILED to recv ws pkt!");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Received ws pkt length:%d %.*s", ws_pkt.len, ws_pkt.len, json_buffer);
    json_t *j = json_parse_static_buffer(ws_pkt.len);
    if (j_find_str(j, "type", tmp_buf64, sizeof(tmp_buf64)) ){
        ESP_LOGI(TAG, "Received ws pkt types %s", tmp_buf64);
    }
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

    va_start (args, msg);
    len = vsnprintf (json_buffer, sizeof(json_buffer) - 64, msg, args);
    va_end (args);

    httpd_resp_set_status(req, "400 Bad Request");

    if (len < sizeof(json_buffer) - 64) {
        jw_init(&jw , json_buffer + len + 1, sizeof(json_buffer) - 1 - len);
        jw_object(&jw){
            jw_kv_str(&jw, "status", "failed");
            jw_kv_str(&jw, "msg", json_buffer);
        }
        request_send_json(req, jw.buf, jw.wptr - jw.buf);
    } else {
        request_send_json(req, "{\"status\":\"failed\"}", 19);
    }
}

static esp_err_t api_v1_get_handler(httpd_req_t *req)
{
    ctx_t *ctx = (ctx_t*) req->user_ctx;
    static json_writer_t jwmem, *jw;
    jw = &jwmem;
    jw_init(jw, json_buffer, sizeof(json_buffer));

    ESP_LOGI(TAG, "%s URI: %s", __func__, req->uri);
    if (strcmp(req->uri, "/api/v1/settings") == 0) {
        if (sft_encode_settings(ctx, jw))
            request_send_json(req, json_buffer, strlen(json_buffer));
        else
            request_send_error(req, "JSON buffer to small - needed:%d", jw->needed_space);
    } else {
        request_send_error(req, "Uri %s not found", req->uri);
    }

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

static esp_err_t api_v1_post_osd(httpd_req_t *req, ctx_t *ctx, const char *uri_tok,
                                 char *post_data, int len)
{
    osd_t *osd = &ctx->osd;
    static json_t jr_obj;
    json_t *jr = &jr_obj;
    static json_writer_t jw;
    int x, y;

    j_init(jr, jsmn_tokens, 128);

    if (streq(uri_tok, "display_text") || streq(uri_tok, "set_text")) {

        ESP_LOGI(TAG, "PARSE json(%d): %.*s", len, len, post_data);
        if(j_parse(jr, post_data, len)) {
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
        } else {
            request_send_error(req, "Failed to parse json");
        }

    } else if (streq(uri_tok, "clear")){
        osd_send_clear(osd);
        request_send_ok(req);

    } else if (streq(uri_tok, "display")){
        osd_send_display(osd);
        request_send_ok(req);

    } else if (streq(uri_tok, "test_format")) {
        if(j_parse(jr, post_data, len)) {
            if (j_find_str(jr, "format", tmp_buf64, sizeof(tmp_buf64))) {
                if (osd_eval_format(osd, tmp_buf64, 23, 1337,666, tmp2_buf64, sizeof(tmp2_buf64))) {
                    jw_init(&jw, json_buffer, sizeof(json_buffer));
                    jw_object(&jw){
                        jw_kv_str(&jw, "status", "ok");
                        jw_kv_str(&jw, "msg", tmp2_buf64);
                    }
                    httpd_resp_set_status(req, "200 OK");
                    request_send_json(req, json_buffer, strlen(json_buffer));
                } else {
                    request_send_error(req, "Invalid OSD message format");
                }
            } else {
                request_send_error(req, "Missing mandatory json field");
            }
        } else {
            request_send_error(req, "Failed to parse json");
        }
    }

    return  ESP_OK;
}

static esp_err_t api_v1_post_time_sync(httpd_req_t *req, ctx_t *ctx,
                                 char *post_data, int len)
{
    static json_t jr_obj, *jr;
    static json_writer_t jw_obj, *jw;

    /* the given post_data, is a pointer into static json_buffer, we will
     * write our responds after it, as we need the mesage while writing
     * the response. */
    char *wbuf =  json_buffer + len + 1;
    int wbuf_len = sizeof(json_buffer) - len - 1;

    json_t client;
    json_t server;
    uint64_t val;

    jr = &jr_obj;
    jw = &jw_obj;

    if (wbuf_len < 0)
        return ESP_ERR_INVALID_SIZE;

    j_init(jr, jsmn_tokens, sizeof(jsmn_tokens) / sizeof(jsmntok_t));

    if (j_parse(jr, post_data, len)) {
        j_find(jr, "server", &server);
        j_find(jr, "client", &client);

        printf("T: start: %.*s\n", len, json_buffer);

        jw_init(jw, wbuf, wbuf_len);
        jw_object(jw) {
            jw_kv(jw, "server"){
                jw_array(jw){
                    json_t e = {0};
                    while(j_next(&server, &e)) {
                        if (j_get_uint64(&e, &val))
                            jw_uint64(jw, val);
                    }
                    jw_uint64(jw, get_millis());
                }
            }
            jw_kv(jw, "client"){
                jw_array(jw){
                    json_t e = {0};
                    while(j_next(&client, &e)) {
                        if (j_get_uint64(&e, &val))
                            jw_uint64(jw, val);
                    }
                }
            }
        }
        if (jw->error) {
            request_send_error(req, "Failed to write json");
        } else {
            httpd_resp_set_status(req, "200 OK");
            request_send_json(req, jw->buf, jw->wptr - jw->buf);
        }
    } else {
            request_send_error(req, "Failed to parse json");
    }

    return ESP_OK;
}


static esp_err_t api_v1_post_handler(httpd_req_t *req)
{
    ctx_t *ctx = (ctx_t*) req->user_ctx;
    lap_counter_t *lc = &ctx->lc;
    static json_writer_t jwmem, *jw;
    static char key[32];
    static char value[32];
    static json_t jr_obj;
    json_t *jr = &jr_obj;
    const char *tok;

    json_buffer[0] = '\0';
    j_init(jr, jsmn_tokens, 128);

    jw = &jwmem;
    jw_init(jw, json_buffer, sizeof(json_buffer));

    if (req->content_len > sizeof(json_buffer)){
        request_send_error(req, "413 Payload Too Large (%d)", req->content_len);
        return ESP_OK;
    }

    int len = httpd_req_recv(req, json_buffer, sizeof(json_buffer));

    ESP_LOGI(TAG, "%s:%d URI: %s data(%d): %.*s", __func__, __LINE__, req->uri, len, len, json_buffer);
    if (strcmp(req->uri, "/api/v1/settings") == 0) {
        if(j_parse(jr, json_buffer, len)) {
            json_t e = {0};
            while(j_next(jr, &e)) {
                if (j_get_kv(&e, key, sizeof(key), value, sizeof(value))) {
                    if (cfg_set_param(&ctx->cfg, key, value) != ESP_OK) {
                        jw_object(jw) {
                            jw_kv_str(jw, "status", "error");
                            jw_kv(jw, "msg") {
                                jw_format(jw, "\"Invalid key/value %s=%s\"", key, value);
                            }
                        }
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
        }  else {
            request_send_error(req, "Failed to parse json");
        }

    } else if (strcmp(req->uri, "/api/v1/start_calibration") == 0) {
        sft_start_calibration(ctx);
        request_send_ok(req);

    } else if (strcmp(req->uri, "/api/v1/clear_laps") == 0) {
        struct player_s *player;

        for (int i=0; i < MAX_PLAYER; i++) {
            player = &lc->players[i];
            memset(player->laps, 0, sizeof(player->laps));
            player->next_idx = 0;

            if (player->ip4.addr != 0 && strlen(player->name) > 0){
                // TODO
                //                    http_send_api_data_to("clear_laps", "", player->ipaddr);
            }
        }

        sft_event_start_race_t ev = {.offset = 30000 };
        ESP_ERROR_CHECK(
            esp_event_post(SFT_EVENT, SFT_EVENT_START_RACE,
                           &ev, sizeof(ev), pdMS_TO_TICKS(500)));

        request_send_ok(req);

    } else if (strcmp(req->uri, "/api/v1/player/connect") == 0) {
        if(j_parse(jr, json_buffer, len)) {
            if (j_find_str(jr, "player", value, sizeof(value))) {
                ip4_addr_t ip4 = {0};
                if (get_remote_ip4(req, &ip4) == ESP_OK) {
                    if (sft_player_get_or_create(lc, ip4, value))
                        request_send_ok(req);
                    else
                        request_send_error(req, "Failed to create player: %s", value);
                } else
                    request_send_error(req, "No remote IP");
            } else
                request_send_error(req, "Missing name");
        } else
            request_send_error(req, "Failed to parse json");

    } else if (strcmp(req->uri, "/api/v1/player/lap") == 0) {
        if(j_parse(jr, json_buffer, len)) {
            json_t lap;
            int id, rssi;
            millis_t duration;
            ip4_addr_t ip4 = {0};

            if (get_remote_ip4(req, &ip4) == ESP_OK &&
                j_find_str(jr, "player", value, sizeof(value)) &&
                j_find(jr, "lap", &lap) &&
                j_find_int(&lap, "id", &id) &&
                j_find_int(&lap, "rssi", &rssi) &&
                j_find_uint64(&lap, "duration", &duration)
            ) {
                if (sft_player_add_lap(sft_player_get_or_create(lc, ip4, NULL),
                                       id, rssi, duration, get_millis()))
                    request_send_ok(req);
                else
                    request_send_error(req, "Failed to add players lap");

            } else
                request_send_error(req, "Failed to parse json");

        } else
            request_send_error(req, "Failed to parse json");

    } else if ((tok = strstartwith(req->uri, "/api/v1/osd/"))) {
        return api_v1_post_osd(req, ctx, tok, json_buffer, len);

    } else if (strcmp(req->uri, "/api/v1/time-sync") == 0) {
        return api_v1_post_time_sync(req, ctx, json_buffer, len);

    } else {
        request_send_error(req, "404 Not found - %s", req->uri);
    }

    return ESP_OK;
}

void sft_event_rssi_update(void* ctx, esp_event_base_t base, int32_t id, void* event_data)
{
    sft_event_rssi_update_t *ev = (sft_event_rssi_update_t*) event_data;
    static json_writer_t jwmem, *jw;

    jw = &jwmem;
    jw_init(jw, json_buffer, sizeof(json_buffer));

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
    gui_send_all(ctx, json_buffer);
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
