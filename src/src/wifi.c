
#include <string.h>
#include "esp_err.h"
#include <freertos/FreeRTOS.h>
#include "esp_wifi_types_generic.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_interface.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include <esp_http_server.h>
#include "esp_event.h"
#include "esp_netif.h"

#if !CONFIG_IDF_TARGET_LINUX
#include <esp_wifi.h>
#include <esp_system.h>
#endif  // !CONFIG_IDF_TARGET_LINUX
//
#include "config.h"

static const char* TAG = "wifi";

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{

}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{

}

void espnow_init(const config_data_t *cfg)
{
    esp_now_peer_info_t peer = {0};

    if (!cfg_has_elrs_uid(cfg))
        return;
    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(espnow_recv_cb) );

    peer.channel = 0;
    peer.encrypt = false;
    memcpy(peer.peer_addr, cfg->elrs_uid, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK( esp_now_add_peer(&peer) );
}

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
}


void wifi_init_softap(const config_data_t *cfg)
{
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 8,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    strcpy((char*)wifi_config.ap.ssid, cfg->ssid);
    wifi_config.ap.ssid_len = strlen(cfg->ssid);

    if (strlen(cfg->passphrase) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    } else {
        strcpy((char*)wifi_config.ap.password, cfg->passphrase);
    }

    if (cfg_has_elrs_uid(cfg)) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        esp_wifi_set_mac(WIFI_IF_STA, cfg->elrs_uid);
    }


    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_config.ap.ssid, wifi_config.ap.password, wifi_config.ap.channel);

    espnow_init(cfg);
}


