#include "msp.h"
#include <string.h>

static uint8_t crc8_dvb_s2(uint8_t crc, unsigned char a)
{
    crc ^= a;
    for (int ii = 0; ii < 8; ++ii) {
        if (crc & 0x80) {
            crc = (crc << 1) ^ 0xD5;
        } else {
            crc = crc << 1;
        }
    }
    return crc;
}

bool msp_crc(msp_packet_t *pkg)
{
    uint8_t crc = 0;
    uint32_t len = sizeof(pkg->h) + pkg->h.payload_sz;
    uint8_t *p = (uint8_t*) &pkg->h;

    if (pkg->h.payload_sz + sizeof(crc) >= MSP_PKG_SIZE)
        return false;

    for (uint32_t i = 0; i < len; i++,p++)
        crc = crc8_dvb_s2(crc, *p);

    *p = crc;
    return true;
}

bool msp_add_byte(msp_packet_t *pkg, uint8_t b)
{

    if (pkg->h.payload_sz + sizeof(uint8_t) + sizeof(uint8_t) >= MSP_PKG_SIZE)
        return false;

    pkg->payload[pkg->h.payload_sz++] = b;
    return true;
}

bool msp_add_str(msp_packet_t *pkg, uint8_t *str, uint32_t len)
{

    if (pkg->h.payload_sz + sizeof(uint8_t) + len >= MSP_PKG_SIZE)
        return false;

    memcpy(&pkg->payload[pkg->h.payload_sz], str, len);
    pkg->h.payload_sz += len;

    return true;
}

uint32_t msp_len(msp_packet_t *pkg)
{
    uint32_t len = ((uint8_t*)&pkg->payload[0]) - (uint8_t*)pkg;
    return len + pkg->h.payload_sz + sizeof(uint8_t) /*CRC*/;
}

bool msp_init(msp_packet_t *pkg, uint8_t type, uint16_t function)
{
    pkg->framing[0] = '$';
    pkg->framing[1] = 'X';

    if (type == MSP_PACKET_RESPONSE) {
        pkg->type = '>';
    } else if (type == MSP_PACKET_COMMAND) {
        pkg->type = '<';
    } else {
        return false;
    }

    pkg->h.flags = 0;
    pkg->h.function = function;
    pkg->h.payload_sz = 0;

    msp_crc(pkg);
    return true;
}


