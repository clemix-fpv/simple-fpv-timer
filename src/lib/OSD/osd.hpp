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
#include <stdlib.h>

#include "msp.h"
#include "msptypes.h"

bool is_zero_mac(unsigned char *mac);

class OSD {
    private:
        MSP msp;
        unsigned char peer_addr[6];
        char send_buffer[64];
        char format[32];
        int last_length;
        uint16_t x;
        uint16_t y;

        bool sendMSPViaEspnow(mspPacket_t *packet);
        int parse_format(const char *format, int *type, int *minlen, int *digits);

    public:
        OSD(unsigned char *peer);
        OSD();
        void set_peer(unsigned char *peer);
        void set_x(uint16_t x){this->x = x;}
        void set_y(uint16_t y){this->y = y;}
        void set_format(const char *format);
        void send_clear();
        void send_display();
        void send_text(uint16_t x, uint16_t y, const char *str);
        void display_text(uint16_t x, uint16_t y, const char *str);
        bool eval_format(const char *format, long lap, long duration, long diff, char *buf, int len);

        bool send_lap(long lap, long duration, long diff);
        
};

