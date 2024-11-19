// SPDX-License-Identifier: GPL-3.0+

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "config.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "string.h"
#include "esp_log.h"
#include "json.h"
#include "esp_random.h"

static const char* TAG = "config";
#define CFG_NVS_KEY "sft-config"

#define config_meta_UINT16(var_name)          \
{                                             \
.name = #var_name,                        \
.type = UINT16,                           \
.size = sizeof(uint16_t),                 \
.offset = offsetof(struct config_data, var_name) \
}

#define config_meta_MACADDR(var_name)         \
{                                             \
.name = #var_name,                          \
.type = MACADDR,                            \
.size = 6,                                  \
.offset = offsetof(struct config_data, var_name)   \
}

#define config_meta_STRING(var_name, length)  \
{                                             \
.name = #var_name,                          \
.type = STRING,                             \
.size = length,                             \
.offset = offsetof(struct config_data, var_name)   \
}

const struct config_meta config_meta[] =
    {
        config_meta_UINT16(freq),
        config_meta_UINT16(rssi_peak),

        config_meta_UINT16(rssi_filter),
        config_meta_UINT16(rssi_offset_enter),
        config_meta_UINT16(rssi_offset_leave),

        config_meta_UINT16(calib_max_lap_count),
        config_meta_UINT16(calib_min_rssi_peak),

        config_meta_MACADDR(elrs_uid),
        config_meta_UINT16(osd_x),
        config_meta_UINT16(osd_y),
        config_meta_STRING(osd_format, 32),

        config_meta_STRING(player_name, 32),

        config_meta_UINT16(wifi_mode),
        config_meta_STRING(ssid, 32),
        config_meta_STRING(passphrase, 32),

        {.name = NULL}
    };


static void initialize_nvs(void)
{
    static unsigned int initialized = 0;
    if (!initialized) {
        ESP_LOGI(TAG, "%s", __func__);
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_ERROR_CHECK(nvs_flash_erase() );
            err = nvs_flash_init();
        }
        ESP_ERROR_CHECK(err);
    }
}

static void cfg_data_init(struct config_data *eeprom)
{
    memset(eeprom, 0, sizeof(*eeprom));
    memcpy(eeprom->magic, CFG_VERSION, 4);
    eeprom->freq = 5658; // R1
    // eeprom->freq = 5917; // R8
    eeprom->rssi_filter = 60;
    eeprom->rssi_offset_enter = 80;
    eeprom->rssi_offset_leave = 70;

    eeprom->wifi_mode = CFG_WIFI_AP;
    cfg_generate_random_ssid(eeprom->ssid, sizeof(eeprom->ssid));

    eeprom->calib_max_lap_count = 3;
    eeprom->calib_min_rssi_peak = 600;

    strcpy(eeprom->osd_format, CFG_DEFAULT_OSD_FORMAT);
}

void cfg_generate_random_ssid(char *buf, size_t len)
{
    snprintf(buf, len, "simple-fpv-timer-%02X", (int)(esp_random() % 0xff));
}

esp_err_t cfg_load(struct config *cfg)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    size_t required_size = sizeof(struct config_data);
    struct config_data cfg_data = {0};


    initialize_nvs();
    err = nvs_open(CFG_NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_get_blob(my_handle, CFG_NVS_KEY, &cfg_data, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
        return err;

    if (required_size == 0) {
        cfg_data_init(&cfg->eeprom);
        return ESP_OK;
    } else {
        cfg->eeprom = cfg_data;
        if (memcmp(cfg_data.magic, CFG_VERSION, sizeof(cfg_data.magic)) != 0) {
            ESP_LOGI(TAG, "Invalid magic %.4s", cfg_data.magic);
            cfg_data_init(&cfg->eeprom);
        }
    }
    ESP_LOGI(TAG, "Loaded configuration:\n");
    cfg_dump(cfg);

    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t cfg_save(struct config *cfg)
{
    esp_err_t err;
    nvs_handle_t fd;

    printf("Save eeprom configuration\n");
    if ((err = nvs_open(CFG_NVS_NAMESPACE, NVS_READWRITE, &fd)) != ESP_OK) {
        printf("FAILED TO OPEN NVS\n");
        return err;
    }

    if ((err = nvs_set_blob(fd, CFG_NVS_KEY, &cfg->eeprom, sizeof(cfg->eeprom))) != ESP_OK) {
        nvs_close(fd);
        return err;
    }

    err = nvs_commit(fd);

    nvs_close(fd);
    return err;
}

esp_err_t cfg_set_param(struct config* cfg, const char *name, const char *value)
{
    const struct config_meta* cm = config_meta;
    for(; cm->name != NULL; cm++) {
        if (strcmp(cm->name, name) == 0) {
            if (cm->type == UINT16) {
                *(uint16_t*)((unsigned char*)&cfg->eeprom + cm->offset) = (uint16_t) atoi(value);
            } else if (cm->type == MACADDR) {
                macaddr_from_str((unsigned char*)&cfg->eeprom + cm->offset, value);
            } else if (cm->type == STRING) {
                size_t len = strlen(value);
                if (len < cm->size) {
                    memcpy((char*)&cfg->eeprom + cm->offset, value, len);
                    memset((char*)&cfg->eeprom + cm->offset + len, 0, cm->size - len);
                } else {
                    memset((char*)&cfg->eeprom + cm->offset, 0, cm->size);
                }
            } else {
                return ESP_ERR_INVALID_ARG;
            }
            return ESP_OK;
        }
    }
    return ESP_ERR_INVALID_ARG;
}

void macaddr_from_str(unsigned char *dst, const char * value)
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

void cfg_dump(struct config * cfg)
{
    const struct config_meta* cm = config_meta;
    const struct config_data *eeprom = &cfg->eeprom;
    const struct config_data *running = &cfg->running;

    printf("CONFIGURATION ---- \n  magic: %.4s\n", eeprom->magic);
    for(; cm->name != NULL; cm++) {
        if (cm->type == UINT16) {
            uint16_t eeprom_value = *(uint16_t*)((unsigned char*)eeprom + cm->offset);
            uint16_t running_value = *(uint16_t*)((unsigned char*)running + cm->offset);

            printf("  %s: %d", cm->name, eeprom_value);
            if (eeprom_value != running_value)
                printf(" [!= %d]", running_value);
            printf("\n");

        } else if (cm->type == MACADDR) {
            const unsigned char *d = (unsigned char*)eeprom + cm->offset;
            const unsigned char *r = (unsigned char*)running + cm->offset;

            printf("  %s: %d,%d,%d,%d,%d,%d", cm->name,
                   d[0], d[1], d[2], d[3], d[4], d[5]);
            if (memcmp(d, r, 6) != 0)
                printf(" [!= %d,%d,%d,%d,%d,%d]",
                   r[0], r[1], r[2], r[3], r[4], r[5]);
            printf("\n");

        } else if (cm->type == STRING) {
            const char *e = (char*)eeprom + cm->offset;
            const char *r = (char*)running + cm->offset;

            printf("  %s: '%s'", cm->name, e);
            if (memcmp(e,r, cm->size) != 0)
                printf(" [!= '%s']", r);
            printf("\n");

        } else {
            printf("  %s: unkown type\n", cm->name);
        }
    }
}

bool cfg_json_encode(struct config_data * cfg, json_writer_t *jw)
{
    const struct config_meta* cm = config_meta;

    jw_object(jw) {
        for(; cm->name != NULL; cm++) {
            if (cm->type == UINT16) {
                jw_kv_int(jw, cm->name, *(uint16_t*)((unsigned char*)cfg + cm->offset));
            } else if (cm->type == MACADDR) {
                jw_kv_mac_in_dec(jw, cm->name, (char *) cfg + cm->offset);
            } else if (cm->type == STRING) {
                jw_kv_str(jw, cm->name, (char *)cfg + cm->offset);
            } else {
                printf("  %s: unkown type\n", cm->name);
            }
        }
    }
    return !jw->error;
}

void cfg_eeprom_to_running(struct config * cfg) {
    cfg->running = cfg->eeprom;
}

bool cfg_changed(struct config* cfg) {
    return memcmp(&cfg->eeprom, &cfg->running, sizeof(cfg->eeprom)) != 0;
}


bool cfg_has_elrs_uid(const config_data_t *cfg)
{
    for (int i = 0; i < 6; i++) {
        if (cfg->elrs_uid[i] != 0)
            return true;
    }
    return false;
}


