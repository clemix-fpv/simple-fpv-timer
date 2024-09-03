// SPDX-License-Identifier: GPL-3.0+

#include <stdlib.h>
#include "osd.h"
#include "msp.h"
#include <esp_now.h>
#include <esp_log.h>
#include <string.h>

static const char * TAG = "OSD";
static msp_packet_t msp_pkt;

bool is_zero_mac(unsigned char *mac)
{
    return *((uint32_t*)&mac[0]) == 0 &&  *((uint16_t*)&mac[4]) == 0;
}

void osd_init(osd_t *osd, unsigned char *peer)
{
    osd->x = osd->y = osd->last_length = 0;
    memcpy(osd->peer_addr, peer, 6);
}

void osd_set_peer(osd_t* osd, unsigned char *peer)
{
    memcpy(osd->peer_addr, peer, 6);
}

void osd_set_format(osd_t* osd, const char *rformat)
{
    snprintf(osd->format, sizeof(osd->format), "%s", rformat);
}

void dump_pkt(uint8_t *buf, uint8_t len)
{
    uint8_t i;

    printf("Buffer len: %d\n", len);
    for (i=0; i < len; i++) {
        if (i > 0 && (i % 8) == 0) {
            printf("\n");
        }
        printf("%02hhx ", buf[i]);
    }
    printf("\n");
}

bool osd_espnow_send(osd_t* osd, msp_packet_t *pkt)
{
    if (is_zero_mac(osd->peer_addr)){
        return false;
    }

    msp_crc(&msp_pkt);
    esp_now_send(osd->peer_addr, (uint8_t *) pkt, msp_len(pkt));
    return true;
}

void osd_display_text(osd_t* osd,uint16_t x, uint16_t y, const char *str)
{
    osd_send_text(osd, x,y,str);
    osd_send_display(osd);
}

void osd_send_text(osd_t* osd, uint16_t x, uint16_t y, const char *str)
{
    msp_init(&msp_pkt, MSP_PACKET_COMMAND, MSP_ELRS_SET_OSD);
    msp_add_byte(&msp_pkt, 3);
    msp_add_byte(&msp_pkt, y);
    msp_add_byte(&msp_pkt, x);
    msp_add_byte(&msp_pkt, 0);
    msp_add_str(&msp_pkt, (uint8_t*)str, strlen(str));

    osd_espnow_send(osd, &msp_pkt);
}

void osd_send_display(osd_t* osd)
{
    msp_init(&msp_pkt, MSP_PACKET_COMMAND, MSP_ELRS_SET_OSD);
    msp_add_byte(&msp_pkt, 4); //display

    osd_espnow_send(osd, &msp_pkt);
}

void osd_send_clear(osd_t* osd)
{
    msp_init(&msp_pkt, MSP_PACKET_COMMAND, MSP_ELRS_SET_OSD);
    msp_add_byte(&msp_pkt, 2); //clear

    osd_espnow_send(osd, &msp_pkt);
}

enum format_types {
	FTYPE_NONE=0,
	FTYPE_PERCENT,
	FTYPE_TIME_MINUTES,
	FTYPE_TIME_SECONDS,
	FTYPE_TIME_MILLIS,
	FTYPE_DELTA_MINUTES,
	FTYPE_DELTA_SECONDS,
	FTYPE_DELTA_MILLIS,
	FTYPE_LAP
};

int osd_parse_format(osd_t* osd,const char *format, int *type, int *minlen, int *digits)
{
	const char *c = format;
	char *i;

	*digits = *minlen = *type = 0;

	if(*c != '%') return 0;
	c++;
	while (*c != '\0'){
		switch (*c){
			case '%':
				c++;
				*type = FTYPE_PERCENT;
				*digits = 0;
				return c-format;
			case '.':
				c++;
				*digits = strtol(c, &i, 10);
				c = i;
				continue;
			case '1': case '2': case '3':
			case '4': case '5': case '6':
			case '7': case '8': case '9':
				*minlen = strtol(c, &i, 10);
				c = i;
				continue;
			case 't':
				c++;
				if (strncmp(c, "ms", 2) == 0){
					c+=2;
					*type = FTYPE_TIME_MILLIS;
				} else if(*c == 'm') {
					c++;
					*type = FTYPE_TIME_MINUTES;
				} else  if (*c == 's') {
					c++;
					*type = FTYPE_TIME_SECONDS;
				} else {
					printf("ERROR: Unknown time format: %s\n", c);
					return -1;
				}
				return c-format;

			case 'd':
				c++;
				if (strncmp(c, "ms", 2) == 0){
					c+=2;
					*type = FTYPE_DELTA_MILLIS;
				} else if(*c == 'm') {
					c++;
					*type = FTYPE_DELTA_MINUTES;
				} else  if (*c == 's') {
					c++;
					*type = FTYPE_DELTA_SECONDS;
				} else {
					printf("ERROR: Unknown time format: %s\n", c);
					return -1;
				}
				return c-format;
			case 'L':
				c++;
				*digits = 0;
				*type = FTYPE_LAP;
				return c-format;
			default:
				printf("ERROR: unexpected character %c\n", *c);
				return -1;
		}
	}
	printf("ERROR: failed to parse %s\n", format);
	return -1;
}

bool osd_eval_format(osd_t* osd,const char *format, int lap, unsigned long long duration, long long diff, char *buf, int len)
{
    const char *c = format;
    char *wptr = buf;
    int j, type, digits, minlen;
    unsigned long long minutes;

#define SAVE_SNPRINTF(...) \
    do{ \
        wptr += snprintf(wptr, len - (wptr-buf), __VA_ARGS__); \
        if (wptr - buf >= len) return false; \
    } while(0)

    if (len <= 0) return false;
    buf[0]=0;
    for(; *c != '\0'; c++){
        if ((j = osd_parse_format(osd, c, &type, &minlen, &digits)) > 0){
            c += j-1;
            switch(type){
                case FTYPE_PERCENT:
                    SAVE_SNPRINTF("%%");
                    break;
                case FTYPE_TIME_SECONDS:
                    SAVE_SNPRINTF("%*.*f", minlen, digits, duration / 1000.0f);
                    break;
                case FTYPE_TIME_MILLIS:
                    SAVE_SNPRINTF("%*.llu", minlen, duration);
                    break;
                case FTYPE_TIME_MINUTES:
                    minutes = duration / 60000;
                    duration -= minutes * 60000;
                    SAVE_SNPRINTF("%llu:%s%.*f", minutes,
                                  duration/1000 < 10 ? "0":"", digits, duration / 1000.0f);
                    break;
                case FTYPE_DELTA_SECONDS:
                    SAVE_SNPRINTF("%+*.*f", minlen, digits, diff / 1000.0f);
                    break;
                case FTYPE_DELTA_MILLIS:
                    SAVE_SNPRINTF("%+*.lld", minlen, diff);
                    break;
                case FTYPE_DELTA_MINUTES:
                    minutes = diff / 60000;
                    diff -= minutes * 60000;
                    if (diff < 0)
                        diff *= -1;
                    SAVE_SNPRINTF("%s%llu:%s%.*f", minutes>0?"+":"",
                                  minutes, diff/1000 < 10 ? "0":"", digits, diff / 1000.0f);
                    break;
                case FTYPE_LAP:
                    SAVE_SNPRINTF("%*.d", minlen, lap);
                    break;
                default:
                    printf("UNKNOWN type: %d\n", type);
            }
            continue;
        } else if (j == 0){
            SAVE_SNPRINTF("%c", *c);
        } else {
            return false;
        }
    }
    return true;
}

bool osd_send_lap(osd_t* osd,long lap, unsigned long long duration, long long diff)
{
    int len, i = 0;

    if (is_zero_mac(osd->peer_addr))
        return false;

    if (!osd_eval_format(osd, osd->format, lap, duration, diff,
                         osd->send_buffer, sizeof(osd->send_buffer))){
        ESP_LOGI(TAG, "[ERROR] Failed to format %s", osd->format);
        return false;
    }
    ESP_LOGI(TAG, "[INFO] sending to osd (%s | %ld, %lld, %lld): %s", osd->format,
            lap, duration, diff, osd->send_buffer);

    len = strlen(osd->send_buffer);
    if (len < osd->last_length) {
        for (i = len; i < osd->last_length && i < sizeof(osd->send_buffer) -1; i++){
            osd->send_buffer[i] = ' ';
        }
        osd->send_buffer[i] = '\0';
    }
    osd->last_length = len;
    osd_display_text(osd, osd->x, osd->y, osd->send_buffer);
    return true;
}


