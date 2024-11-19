// SPDX-License-Identifier: GPL-3.0+
#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "json.h"

#define CFG_DEFAULT_OSD_FORMAT  "%2L: %5.2ts(%6.2ds)"
#define CFG_VERSION             "v002"
#define CFG_NVS_NAMESPACE       "config"

#define CFG_WIFI_MODE_AP        0
#define CFG_WIFI_MODE_STA       1

typedef enum { CFG_WIFI_AP, CFG_WIFI_STA} cfg_wifi_mode_enum;

typedef struct config_data config_data_t;
struct config_data {
        char magic[4];
        uint16_t freq;
        uint16_t rssi_peak;

        uint16_t rssi_filter; /* Smooth rssi input signals, Range: 1-100, a value near to 1 smooth more a value of 100 keeps the raw RSSI value */
        uint16_t rssi_offset_enter; /* The percentage of Peak-RSSI to count a drone entered the gate range: 50-100 */
        uint16_t rssi_offset_leave; /* The percentage of Peak-RSSI to count a drone leave the gate range: 50-100 */

        uint16_t calib_max_lap_count;
        uint16_t calib_min_rssi_peak;

        char player_name[32];

        uint8_t elrs_uid[6];
        uint16_t osd_x;
        uint16_t osd_y;
        char osd_format[32];

        uint16_t wifi_mode;
        char ssid[32];
        char passphrase[32];
};

typedef struct config config_t;
struct config {
  struct config_data running;
  struct config_data eeprom;
};



enum config_type { UINT16, MACADDR, STRING};
struct config_meta {
    const char *name;
    enum config_type type;
    size_t size;
    unsigned int offset;
};

void macaddr_from_str(unsigned char*, const char*);

esp_err_t cfg_set_param(struct config*, const char*, const char*);
esp_err_t cfg_save(struct config*);
esp_err_t cfg_load(struct config*);
void cfg_eeprom_to_running(struct config*);

void cfg_generate_random_ssid(char *buf, size_t len);

bool cfg_has_elrs_uid(const config_data_t *cfgdata);

bool cfg_changed(struct config*);
const struct config_meta* cfg_meta();

void cfg_dump(struct config*);
bool cfg_json_encode(struct config_data * cfg, json_writer_t *jw);
