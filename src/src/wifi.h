// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "config.h"
#include "esp_now.h"
#include "esp_netif.h"

typedef struct {
    enum {
        WIFI_NONE = 0,
        WIFI_AP,
        WIFI_STA,
    } state;

    esp_now_peer_info_t peer;
    esp_netif_t *netif_ap;
    esp_netif_t *netif_sta;

} wifi_t;

void wifi_setup(wifi_t *wifi, const config_data_t *cfg);
