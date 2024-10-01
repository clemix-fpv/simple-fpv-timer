// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <stdbool.h>
#include <inttypes.h>

#define MSP_PKG_SIZE 64

#define MSP_ELRS_SET_OSD                        0x00B6

typedef enum {
    MSP_PACKET_UNKNOWN,
    MSP_PACKET_COMMAND,
    MSP_PACKET_RESPONSE
} mspPacketType_e;

/**
 * MSP V2 Message Structure:
 * Offset  Usage          In CRC   Comment:
 * ======= ======         =======  ========
 * 0       '$'                     Framing magic start char
 * 1       'X'                     'X' in place of v1 'M'
 * 2       type                    '<' / '>' / '!' Message Type
 * 3       flag           +        uint8,
 * 4       function       +        uint16 (little endian).
 *                                 0 - 255 is the same function as V1 for backwards compatibility
 * 6       payload size   +        uint16 (little endian) payload size in bytes
 * 8       payload        +        n (up to 65535 bytes) payload
 * n+8     checksum                uint8, (n= payload size), crc8_dvb_s2 checksum
 */
typedef struct __attribute__((packed)) {
    char framing[2];
    char type;
    struct  __attribute__((packed)) {
        uint8_t  flags;
        uint16_t function;
        uint16_t payload_sz;
    } h;
    uint8_t payload[MSP_PKG_SIZE];
} msp_packet_t;


/**
 * Initialize the given msp package and set type and function.
 * e.g.
 *   msp_packet_t p;
 *   msp_init(&p, MSP_PACKET_COMMAND, MSP_ELRS_SET_OSD);
 */
bool msp_init(msp_packet_t *pkg, uint8_t type, uint16_t function);

/**
 * Retrieves the current length of the full (framing + header +
 * payload + crc) msp packet
 */
uint32_t msp_len(msp_packet_t *pkg);

/**
 * Add a sequence of bytes to the given msp packet.
 * Return false on error.
 */
bool msp_add_str(msp_packet_t *pkg, uint8_t *str, uint32_t len);

/**
 * Add one byte of payload to the given msp packet
 * Return false on error.
 */
bool msp_add_byte(msp_packet_t *pkg, uint8_t b);

/**
 * Recalculate the crc of current msp packet
 * Return false on error.
 */
bool msp_crc(msp_packet_t *pkg);
