// SPDX-License-Identifier: GPL-3.0+
#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <stdbool.h>
#include <stdint.h>
#include "json.h"

#define CFG_DEFAULT_OSD_FORMAT  "%2L: %5.2ts(%6.2ds)"
#define CFG_VERSION             "v005"
#define CFG_NVS_NAMESPACE       "config"

#define CFG_WIFI_MODE_AP        0
#define CFG_WIFI_MODE_STA       1

#define CFG_GAME_MODE_RACE      0
#define CFG_GAME_MODE_CTF       1
#define CFG_GAME_MODE_SPECTRUM  2

#define CFG_LED_MODE_RSSI       0
#define CFG_LED_MODE_RACE       1
#define CFG_LED_MODE_PATTERN_1  2

#define CFG_MAX_FREQ            8

typedef enum { CFG_WIFI_AP = 0, CFG_WIFI_STA = 1} cfg_wifi_mode_enum;
typedef struct config_rssi config_rssi_t;
typedef struct config_data config_data_t;

struct config_rssi {
    uint16_t freq;   /* the frequency this rssi set belongs to */
    uint16_t peak;

    uint16_t filter; /* Smooth rssi input signals, Range: 1-100, a value near to 1 smooth more a value of 100 keeps the raw RSSI value */
    uint16_t offset_enter; /* The percentage of Peak-RSSI to count a drone entered the gate range: 50-100 */
    uint16_t offset_leave; /* The percentage of Peak-RSSI to count a drone leave the gate range: 50-100 */

    uint16_t calib_max_lap_count;
    uint16_t calib_min_rssi_peak;

    uint32_t led_color; /* used for capture the flag team LED color */

    char name[32];
};

struct config_data {
    char magic[4];

    config_rssi_t rssi[CFG_MAX_FREQ];


    uint8_t elrs_uid[6];
    uint16_t osd_x;
    uint16_t osd_y;
    char osd_format[32];

    uint16_t wifi_mode;
    char ssid[32];
    char passphrase[32];

    uint16_t game_mode;

    uint16_t led_num;
    uint16_t race_start_offset;
};

typedef struct config config_t;
struct config {
  struct config_data running;
  struct config_data eeprom;
};



enum config_type { UINT16, UINT32, MACADDR, STRING};
struct config_meta {
    const char *name;
    enum config_type type;
    size_t size;
    unsigned int offset;
};

void macaddr_from_str(unsigned char*, const char*);

esp_err_t cfg_set_param(struct config*, const char*, const char*);
esp_err_t cfg_verify(struct config*);
esp_err_t cfg_save(struct config*);
esp_err_t cfg_load(struct config*);
void cfg_eeprom_to_running(struct config*);

void cfg_generate_random_ssid(char *buf, size_t len);

bool cfg_has_elrs_uid(const config_data_t *cfgdata);

bool cfg_changed(struct config*);
const struct config_meta* cfg_meta();

void cfg_dump(struct config*);
bool cfg_json_encode(struct config_data * cfg, json_writer_t *jw);
