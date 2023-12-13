// SPDX-License-Identifier: GPL-3.0+

#include <stdlib.h>
#include <osd.hpp>
#include <logging.hpp>
#include <esp_now.h>

bool is_zero_mac(unsigned char *mac)
{
    return *((uint32_t*)&mac[0]) == 0 &&  *((uint16_t*)&mac[4]) == 0;
}

OSD::OSD(unsigned char *peer)
{
    x = y = last_length = 0;
    memcpy(peer_addr, peer, 6);
}

OSD::OSD()
{
    x = y = last_length = 0;
    memset(peer_addr, 0, sizeof(peer_addr));
}

void OSD::set_peer(unsigned char *peer)
{
    memcpy(peer_addr, peer, 6);
}

void OSD::set_format(const char *rformat)
{
    DBGLN("SET format: %s", rformat);
    snprintf(format, sizeof(format), "%s", rformat);
}

bool OSD::sendMSPViaEspnow(mspPacket_t *packet)
{
    uint8_t packetSize = msp.getTotalPacketSize(packet);
    uint8_t i;
    uint8_t nowDataOutput[packetSize];

    if (is_zero_mac(peer_addr)){
        return false;
    }

    if (!msp.convertToByteArray(packet, nowDataOutput)) {
        // packet could not be converted to array, bail out
        DBGLN("Failed to marschal package!");
        return false;
    }

    esp_now_send(peer_addr, (uint8_t *) &nowDataOutput, packetSize);
    return true;
}

void OSD::display_text(uint16_t x, uint16_t y, const char *str)
{
    int len, i;

    len = strlen(str);
    if (len < last_length) {
        if (str != send_buffer){
            snprintf(send_buffer, sizeof(send_buffer), "%s", str);
            str = send_buffer;
        }
        for (i = len; i < last_length && i < sizeof(send_buffer) -1; i++){
            send_buffer[i] = ' ';
        }
    }

    send_text(x,y,str);
    send_display();
}

void OSD::send_text(uint16_t x, uint16_t y, const char *str)
{
    unsigned int i;
    mspPacket_t pkt;

    pkt.reset();

    pkt.makeCommand();
    pkt.function =  MSP_ELRS_SET_OSD;


    pkt.addByte(3); // set text
    pkt.addByte(y);
    pkt.addByte(x);
    pkt.addByte(0);
    
    for (i = 0; i < MSP_PORT_INBUF_SIZE; i++) {
        if (str[i] == 0) break;
        pkt.addByte(str[i]);
    }

    sendMSPViaEspnow(&pkt);
}

void OSD::send_display()
{
    mspPacket_t pkt;

    pkt.reset();

    pkt.makeCommand();
    pkt.function =  MSP_ELRS_SET_OSD;

    pkt.addByte(4); // set text
    sendMSPViaEspnow(&pkt);
}

void OSD::send_clear()
{
    mspPacket_t pkt;

    pkt.reset();

    pkt.makeCommand();
    pkt.function =  MSP_ELRS_SET_OSD;

    pkt.addByte(2); // set text
    sendMSPViaEspnow(&pkt);
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

int OSD::parse_format(const char *format, int *type, int *minlen, int *digits)
{
	const char *c = format;
	char *i;

	*digits = *minlen = *type = 0;

	if(*c != '%') return -1;
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
					break;
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

bool OSD::eval_format(const char *format, long lap, long duration, long diff, char *buf, int len)
{
    const char *c = format;
    char *wptr = buf;
    int j, type, digits, minlen;
    long minutes;

#define SAVE_SNPRINTF(...) \
    do{ \
	wptr += snprintf(wptr, len - (wptr-buf), __VA_ARGS__); \
	if (wptr - buf >= len) return false; \
    } while(0)

    if (len <= 0) return false;
    buf[0]=0;
    for(; *c != '\0'; c++){
	    if ((j = parse_format(c, &type, &minlen, &digits)) > 0){
		    c += j-1;
		    switch(type){
			    case FTYPE_PERCENT:
				    SAVE_SNPRINTF("%%");
				    break;
			    case FTYPE_TIME_SECONDS:
				    SAVE_SNPRINTF("%*.*f", minlen, digits, duration / 1000.0f);
				    break;
			    case FTYPE_TIME_MILLIS:
				    SAVE_SNPRINTF("%*.d", minlen, duration);
				    break;
			    case FTYPE_TIME_MINUTES:
				    minutes = duration / 60000;
				    duration -= minutes * 60000;
				    SAVE_SNPRINTF("%d:%s%.*f", minutes, duration/1000 < 10 ? "0":"", digits, duration / 1000.0f);
				    break;
			    case FTYPE_DELTA_SECONDS:
				    SAVE_SNPRINTF("%+*.*f", minlen, digits, diff / 1000.0f);
				    break;
			    case FTYPE_DELTA_MILLIS:
				    SAVE_SNPRINTF("%+*.d", minlen, diff);
				    break;
			    case FTYPE_DELTA_MINUTES:
				    minutes = diff / 60000;
				    diff -= minutes * 60000;
				    if (diff < 0) 
					diff *= -1;
				    SAVE_SNPRINTF("%s%d:%s%.*f", minutes>0?"+":"", minutes, diff/1000 < 10 ? "0":"", digits, diff / 1000.0f);
				    break;
			    case FTYPE_LAP:
				    SAVE_SNPRINTF("%*.d", minlen, lap);
				    break;
			    default:
				    printf("UNKNOWN type: %d\n", type);
		    }
		    continue;
	    } else {
		SAVE_SNPRINTF("%c", *c);
	    }
    }
    return true;
}
    
bool OSD::send_lap(long lap, long duration, long diff)
{
    int len, i;

    if (is_zero_mac(peer_addr))
        return false;

    if (!eval_format(format, lap, duration, diff, send_buffer, sizeof(send_buffer))){
        DBGLN("[ERROR] Failed to format %s", format);
        return false;
    }
    DBGLN("[INFO] sending to osd (%s | %ld, %ld, %ld): %s", format, 
            lap, duration, diff, send_buffer);

    len = strlen(send_buffer);
    if (len < last_length) {
        for (i = len; i < last_length && i < sizeof(send_buffer) -1; i++){
            send_buffer[i] = ' ';
        }
    }
    send_buffer[i] = '\0';
    last_length = len;
    display_text(x, y, send_buffer);
    return true;
}


