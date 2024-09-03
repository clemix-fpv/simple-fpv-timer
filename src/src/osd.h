/*
 * =====================================================================================
 *
 *       Filename:  sendmsp.hpp
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  22.08.2023 23:03:01
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (), 
 *   Organization:  
 *
 * =====================================================================================
 */

#pragma once
#include <stdbool.h>
#include <inttypes.h>

bool is_zero_mac(unsigned char *mac);

typedef struct {
        unsigned char peer_addr[6];
        char send_buffer[64];
        char format[32];
        int last_length;
        uint16_t x;
        uint16_t y;
} osd_t;

void osd_init(osd_t *, unsigned char *peer);
void osd_set_peer(osd_t *, unsigned char *peer);
void osd_set_format(osd_t *,const char *format);
void osd_send_clear(osd_t *);
void osd_send_display(osd_t *);
void osd_send_text(osd_t *, uint16_t x, uint16_t y, const char *str);
void osd_display_text(osd_t *, uint16_t x, uint16_t y, const char *str);
bool osd_eval_format(osd_t* osd,const char *format, int lap, unsigned long long duration, long long diff, char *buf, int len);

bool osd_send_lap(osd_t *,long lap, unsigned long long duration, long long diff);

