// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <lwip/ip4_addr.h>
#include <stdint.h>
#define JSMN_PARENT_LINKS
#define JSMN_HEADER
#include "jsmn.h"

typedef struct  {
    jsmntok_t *tokens;  /* Pointer to the first token of a JSON object */
    jsmntok_t *end;            /* Number of tokens in *tokens */
    const char *buf;
} json_t;

void j_init(json_t *j, jsmntok_t *tokens, size_t num);
json_t * j_parse(json_t *j, const char *in, size_t len);
void j_print(json_t * j);
json_t * j_find(json_t *j, const char *name, json_t *result);
jsmntok_t * j_get_kv(json_t *j, char *key, size_t klen, char *value, size_t vlen);
jsmntok_t * j_get_str(json_t *j, char *buf, size_t len);
int j_eq_str(json_t *j, char *str);
jsmntok_t * j_get_int(json_t *j, int *ret);
jsmntok_t * j_find_str(json_t *j, const char *name, char *buf, size_t len);;
jsmntok_t * j_find_int(json_t *j, const char *name, int *ret);
jsmntok_t * j_find_uint64(json_t *j, const char *name, uint64_t *ret);
json_t * j_next(json_t * array, json_t *prev);
json_t * j_value(json_t * node, json_t *ret);


typedef struct {
    char *buf;
    size_t len;
    char *wptr;
    bool error;
    size_t needed_space;
} json_writer_t;


void jw_init(json_writer_t *jw, char *buf, size_t len);

#define jw_object(jw)   for(jw_object_start(jw);                        \
                            jw_prev(jw) != '}' && jw_can_write(jw, 1);  \
                            jw_object_end(jw) )
void jw_object_start(json_writer_t * jw);
void jw_object_end(json_writer_t * jw);

#define jw_array(jw)    for(jw_array_start(jw);                         \
                            jw_prev(jw) != ']'&& jw_can_write(jw, 1);   \
                            jw_array_end(jw) )
void jw_array_start(json_writer_t * jw);
void jw_array_end(json_writer_t * jw);

#define jw_kv(jw, key)  for(jw_kv_start(jw, key);                       \
                            jw_prev(jw) != ','&& jw_can_write(jw, 1);   \
                            jw_kv_end(jw) )
void jw_kv_start(json_writer_t *jw, const char *key);
void jw_kv_end(json_writer_t *jw);

void jw_str(json_writer_t *jw, const char *value);
void jw_int(json_writer_t *jw, int value);
void jw_format(json_writer_t *jw, const char *format, ...);
void jw_kv_str(json_writer_t *jw, const char *key, const char *value);
void jw_kv_int(json_writer_t *jw, const char *key, int value);
void jw_kv_bool(json_writer_t *jw, const char *key, bool value);
void jw_kv_ip4(json_writer_t *jw, const char *key, ip4_addr_t ipv4);
void jw_kv_mac_in_dec(json_writer_t *jw, const char *key, const char *mac);

char jw_prev(json_writer_t *jw);
bool jw_can_write(json_writer_t *jw, size_t needed);
