// SPDX-License-Identifier: GPL-3.0+

#include <stdlib.h>
#include <logging.hpp>
#include "config.hpp"

const char *Config::MAGIC = "c001";

#define config_meta_UINT(var_name) {.name = #var_name, .type = UINT, .size = 0, .offset = offsetof(config_s, var_name)}
#define config_meta_MACADDR(var_name) {.name = #var_name, .type = MACADDR, .size = 6, .offset = offsetof(config_s, var_name)}
#define config_meta_ENUM(var_name, max_val) {.name = #var_name, .type = ENUM, .size = max_val, .offset = offsetof(config_s, var_name)}
#define config_meta_STRING(var_name, length) {.name = #var_name, .type = STRING, .size = length, .offset = offsetof(config_s, var_name)}
const struct config_meta_s config_meta[] =
 {
    config_meta_UINT(freq),
    config_meta_UINT(rssi_peak),

    config_meta_UINT(rssi_filter),
    config_meta_UINT(rssi_offset_enter),
    config_meta_UINT(rssi_offset_leave),

    config_meta_UINT(calib_max_lap_count),
    config_meta_UINT(calib_min_rssi_peak),

    config_meta_STRING(player_name, 32),

    config_meta_UINT(wifi_mode),
    config_meta_STRING(ssid, 32),
    config_meta_STRING(passphrase, 32),

    {.name = NULL}
};


std::string config2json(struct config_s *cfg)
{
    std::stringstream ss;
    const struct config_meta_s* cm = config_meta;
    bool first = true;
    unsigned char *mac;

    ss << "{" << std::endl;
    
    for(; cm->name != NULL; cm++) {
        if(!first)
            ss << "," << std::endl; 
        first = false;

        switch (cm->type){
            case UINT:
                ss << "\"" << cm->name << "\": " << *(uint16_t*)((unsigned char*)cfg + cm->offset); 
                break;
            case STRING:
                ss << "\"" << cm->name << "\": \"" << (char*)((unsigned char*)cfg + cm->offset) << "\"";
                break;
            case ENUM:
                ss << "\"" << cm->name << "\": " << *(cfg_wifi_mode_enum*)((unsigned char*)cfg + cm->offset);
                break;
            case MACADDR:
                ss << "\"" << cm->name << "\": \"";
                mac = (unsigned char*)cfg + cm->offset;
                for(int i = 0; i < 6; i++){
                   ss << (mac[i] & 0xff);
                   if (i < 5)
                       ss << ",";
                }
                ss << "\"";
                break;
            default:
                DBGLN("ERROR unknonwn type in config2json() - type: %d", cm->type);
                first = true; /*  do not add a ',' on next loop */
        }
    }
    ss << "}" << std::endl;

    return ss.str();
}


Config::Config()
{
    memset(&eeprom, 0, sizeof(eeprom));
    memcpy(eeprom.magic, Config::MAGIC, 4);
    eeprom.freq = 5658; // R1
    eeprom.freq = 5917; // R8
    eeprom.rssi_filter = 60;
    eeprom.rssi_offset_enter = 80;
    eeprom.rssi_offset_leave = 70;

    eeprom.wifi_mode = CFG_WIFI_AP;

    eeprom.calib_max_lap_count = 3;
    eeprom.calib_min_rssi_peak = 600;
}

void Config::parse_macaddr(unsigned char *dst, const char * value)
{
    unsigned char mac[6];
    memset(mac, 0, 6);

    if (sscanf(value, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
        memcpy(dst, mac, 6);
    } else if (sscanf(value, "%hhd,%hhd,%hhd,%hhd,%hhd,%hhd",
                &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) == 6) {
        memcpy(dst, mac, 6);
    }
}

void Config::setParam(const char *name, const char *value)
{
    const struct config_meta_s* cm = config_meta;
    for(; cm->name != NULL; cm++) {
        DBGLN("ALL SetParam: %s offset:%ld size:%d type:%d", cm->name, cm->offset, cm->size, cm->type);
        if (strcmp(cm->name, name) == 0) {
            if (cm->type == UINT) {
                *(uint16_t*)((unsigned char*)&eeprom + cm->offset) = (uint16_t) std::atoi(value);
            } else if (cm->type == MACADDR) {
                parse_macaddr((unsigned char*)&eeprom + cm->offset, value);
            } else if (cm->type == STRING) {
                DBGLN("SetParam: %s offset:%ld size:%d", name, cm->offset, cm->size);
                if (strlen(value) < cm->size) {
                    memcpy((char*)&eeprom + cm->offset, value, strlen(value)+1);
                } else {
                    memset((char*)&eeprom + cm->offset, 0, cm->size);
                }
            }
        }
    }
}

void Config::save() {
    DBGLN("SAVE CONFIGURAION!");
    EEPROM.put(0, eeprom);
    EEPROM.commit();
}

void Config::setup() {
    EEPROM.begin(256);
}

void Config::load() {
    config_s tmp; 
    int i;
    EEPROM.get(0, tmp);
    if (memcmp(tmp.magic, Config::MAGIC, 4) == 0) {
        DBGLN("Configuration loaded!");
        eeprom = tmp;
        dump();
    } else {
        DBGLN("Use default config!");
        dump();
    }
}

void Config::dump() {
    const struct config_meta_s* cm = config_meta;
    DBGLN("CONFIGURATION ---- \n  magic: %.4s", eeprom.magic);
    for(; cm->name != NULL; cm++) {
        if (cm->type == UINT) {
            DBGLN("  %s: %d", cm->name, *(uint16_t*)((unsigned char*)&eeprom + cm->offset));
        } else if (cm->type == MACADDR) {
            const unsigned char *d = (unsigned char*)&eeprom + cm->offset;
            DBGLN("  %s: %d,%d,%d,%d,%d,%d", cm->name, d[0], d[1], d[2], d[3], d[4], d[5]);
        } else if (cm->type == STRING) {
            DBGLN("  %s: %s", cm->name, (unsigned char*)&eeprom + cm->offset);
        } else if (cm->type == ENUM) {
            DBGLN("  %s: %d", cm->name, *(cfg_wifi_mode_enum*)(unsigned char*)&eeprom + cm->offset);
        } else {
            DBGLN("  %s: unkown type");
        }
    }
}

void Config::setRunningCfg() {
    running = eeprom;
}

bool Config::changed() {
    return memcmp(&eeprom, &running, sizeof(eeprom)) != 0;
}

