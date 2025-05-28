// SPDX-License-Identifier: GPL-3.0+
#pragma once

#include <freertos/FreeRTOS.h>
#include <esp_err.h>
#include <stdbool.h>
#include "config_data.h"
#include "json.h"

typedef struct config config_t;
struct config {
    struct config_data running;
    struct config_data eeprom;
};



enum config_type { INT16, UINT16, UINT32, MACADDR, STRING, IPV4};
struct config_meta {
    const char *name;
    enum config_type type;
    size_t size;
    unsigned int offset;
};

void macaddr_from_str(unsigned char*, const char*);

esp_err_t cfg_data_set_param(config_data_t* data, const char *name, const char *value);
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
bool cfg_meta_json_encode(struct config_data * cfg, const struct config_meta *meta, json_writer_t *jw);

/* Helper makros for config eeprom/running handling */
#define cfg_differ_str(cfg, field) \
    (memcmp((cfg)->eeprom.field, (cfg)->running.field, sizeof(cfg->running.field)) != 0)

#define cfg_set_running_str(cfg, field) \
    memcpy((cfg)->running.field, (cfg)->eeprom.field, sizeof((cfg)->running.field))

#define cfg_differ(cfg, field) \
    (memcmp(&(cfg)->eeprom.field, &(cfg)->running.field, sizeof((cfg)->running.field)) != 0)

#define cfg_set_running(cfg, field) \
    memcpy(&(cfg)->running.field, &(cfg)->eeprom.field, sizeof((cfg)->running.field))


