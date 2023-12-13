// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include <string.h>
#include <sstream>
#include <cstddef>
#include <iostream>


typedef enum { CFG_WIFI_AP, CFG_WIFI_STA} cfg_wifi_mode_enum;

struct config_s {
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


enum config_type { UINT, MACADDR, STRING, ENUM};
struct config_meta_s {
    const char *name;
    enum config_type type;
    size_t size;
    unsigned int offset;
};

extern const struct config_meta_s config_meta[];

std::string config2json(struct config_s *cfg);


class Config {
    public:
        static const char *MAGIC;
        static const char *DEFAULT_OSD_FORMAT;

        struct config_s eeprom;
        struct config_s running;

        Config();


        void parse_macaddr(unsigned char *dst, const char * value);
        void setParam(const char *name, const char *value);


        void save();
        void setup();
        void load();
        void dump();
        void setRunningCfg();
        bool changed();
};
