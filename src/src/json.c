// SPDX-License-Identifier: GPL-3.0+

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <lwip/ip4_addr.h>
#include <lwip/sockets.h>
#define JSMN_PARENT_LINKS
#include "jsmn.h"
#include "json.h"

void j_print(json_t * j)
{
    jsmntok_t *t = j->tokens;
    printf("t:%p '%.*s' parent:%d size:%d type:%d\n",
           t, t->end - t->start, j->buf + t->start, t->parent, t->size, t->type);
}

void j_init(json_t *j, jsmntok_t *tokens, size_t num)
{
    j->tokens = tokens;
    j->end = tokens + num;
    j->buf = NULL;
}

json_t * j_parse(json_t *j, const char *in, size_t len)
{
    jsmn_parser p;
    int num;

    jsmn_init(&p);
    num = jsmn_parse(&p, in, len, j->tokens, j->end - j->tokens);
    if (num > 0) {
        j->buf = in;
        j->end = j->tokens + num;
        return j;
    }
    return NULL;
}

json_t * j_find(json_t *j, const char *name, json_t *result)
{
    jsmntok_t *t = j->tokens;
    jsmntok_t *end = j->end;

    memset(result, 0, sizeof(*result));

    if ((end - t) < 2)
        return NULL;

    int parent = t[1].parent;
    for (; t < end; t++) {

        if (parent == -1)
            parent = t->parent;

        if (t->parent != parent)
            continue;

        if (t->type == 0)
            continue;

        if (strncmp(j->buf + t->start, name, t->end - t->start) != 0)
            continue;

        /* token found, take the next as it is the value */
        t++;
        result->tokens = t;
        result->end = end;
        result->buf = j->buf;
        return result;
    }
    return NULL;
}

jsmntok_t * j_get_kv(json_t *j, char *key, size_t klen, char *value, size_t vlen)
{
    jsmntok_t *t;
    json_t v = {0};

    if ((t = j_get_str(j, key, klen)) &&
                j_next(j, &v) &&
                j_get_str(&v, value, vlen) ){
        return t;
    }
    return NULL;
}

jsmntok_t * j_get_str(json_t *j, char *buf, size_t len)
{
    jsmntok_t *t;

    t = j->tokens;


    if (!t || t->type != JSMN_STRING)
        return NULL;

    int t_len = t->end - t->start;
    if (buf) {
        if (t_len < len) {
            for(int i = 0; i < t_len; i++) {
                char c = j->buf[i + t->start];
                if (c != '\\') {
                    *buf = c;
                    buf++;
                } else {
                    i++;
                    if (i < t_len) {
                        *buf = j->buf[i + t->start];
                        buf++;
                    }
                }
            }
            *buf = 0;
        } else {
            errno = ENOBUFS;
            return NULL;
        }
    }
    return t;
}

int j_eq_str(json_t *j, char *str)
{
    jsmntok_t *t;

    t = j->tokens;

    if (!t || t->type != JSMN_STRING || !str)
        return 0;

    int t_len = t->end - t->start;

    for(int i = 0; i < t_len; i++) {

        char c = j->buf[i + t->start];
        if (c == '\\') {
            i++;
            if (i < t_len) {
                c = j->buf[i + t->start];
            }
        }
        if (c != *str)
            return 0; /* is not equal */
        str++;
    }
    return *str == '\0';
}



jsmntok_t * j_get_int(json_t *j, int *ret)
{
    jsmntok_t *t;

    t = j->tokens;

    if (!t || (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE))
        return NULL;

    *ret = atoi(j->buf + t->start);
    return t;
}

jsmntok_t * j_get_long(json_t *j, long *ret)
{
    jsmntok_t *t;

    t = j->tokens;

    if (!t || (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE))
        return NULL;

    *ret = atol(j->buf + t->start);
    return t;
}

jsmntok_t * j_get_uint64(json_t *j, uint64_t *ret)
{
    jsmntok_t *t;

    t = j->tokens;

    if (!t || (t->type != JSMN_STRING && t->type != JSMN_PRIMITIVE))
        return NULL;

    if (sizeof(unsigned long) == 8) {
        *ret = strtoul(j->buf + t->start, NULL, 10);
    } else {
        *ret = strtoull(j->buf + t->start, NULL, 10);
    }
    return t;
}

jsmntok_t * j_find_str(json_t *j, const char *name, char *buf, size_t len)
{
    json_t f;

    if (!j_find(j, name, &f))
        return NULL;

    return j_get_str(&f, buf, len);
}

jsmntok_t * j_find_int(json_t *j, const char *name, int *ret)
{
    json_t f;

    if (!j_find(j, name, &f))
        return NULL;

    return j_get_int(&f, ret);
}

jsmntok_t * j_find_uint64(json_t *j, const char *name, uint64_t *ret)
{
    json_t f;

    if (!j_find(j, name, &f))
        return NULL;

    return j_get_uint64(&f, ret);
}

json_t * j_next(json_t * array, json_t *prev)
{
    if (!prev->tokens){
        if (array->end - array->tokens == 0)
            return NULL;
        prev->tokens = &array->tokens[1];
        prev->end = array->end;
        prev->buf = array->buf;
        return prev;
    }

    int parent = prev->tokens[0].parent;
    jsmntok_t *t = prev->tokens;
    for (t++; t < prev->end; t++){
        if (parent != t->parent)
            continue;
        prev->tokens = t;
        return prev;
    }

    return NULL;
}

json_t * j_value(json_t * node, json_t *ret)
{
    int parent = node->tokens[0].parent;
    if (node->tokens[1].parent != parent) {
        ret->tokens = &node->tokens[1];
        ret->end = node->end;
        ret->buf = node->buf;
        return ret;
    }

    return NULL;
}

void jw_init(json_writer_t *jw, char *buf, size_t len)
{
    if (len > 0)
        buf[0] = '\0';
    jw->buf = buf;
    jw->len = len;
    jw->wptr = buf;
    jw->error = 0;
    jw->needed_space = 0;
}

bool jw_can_write(json_writer_t *jw, size_t needed)
{
    if (jw->error != 0) {
        jw->needed_space += needed;
        return false;
    }

    if (((jw->wptr - jw->buf) + needed) < jw->len) {
        return true;
    } else  {
        jw->needed_space = jw->wptr - jw->buf;
        jw->needed_space += needed;
        snprintf(jw->buf, jw->len, "{ \"error\": \"JSON buffer to small\"}");
        jw->error = ENOBUFS;
    }
    return false;
}

char jw_prev(json_writer_t *jw)
{
    if (jw->wptr > jw->buf){
        return *(jw->wptr - 1);
    }
    return '\0';
}

static void jw_put(json_writer_t *jw, char c)
{
   if (!jw_can_write(jw, 2))
        return;
    *jw->wptr = c;
    *(jw->wptr+1) = '\0';
    jw->wptr++;
}

static void jw_write_quoted_buf(json_writer_t *jw, const char *buf, size_t len)
{
    jw_put(jw, '"');
    for (int i=0; i < len; i++) {
        if (buf[i] == '"' || buf[i] == '\\') {
            jw_put(jw, '\\');
        }
        jw_put(jw, buf[i]);
    }
    jw_put(jw, '"');
}

static void jw_write_quoted(json_writer_t *jw, const char *str)
{
    jw_write_quoted_buf(jw, str, strlen(str));
}

void jw_object_start(json_writer_t * jw)
{
    switch (jw_prev(jw)) {
        case '\0':
        case ':':
        case '[':
        case '{':
            break;
        default:
            jw_put(jw, ',');
    }

    jw_put(jw, '{');
}
void jw_object_end(json_writer_t * jw)
{
    if (jw_prev(jw) == ',') jw->wptr--;
    jw_put(jw, '}');
}

void jw_array_start(json_writer_t * jw)
{
    switch (jw_prev(jw)) {
        case '\0':
        case ':':
        case '[':
        case '{':
            break;
        default:
            jw_put(jw, ',');
    }

    jw_put(jw, '[');
}

void jw_array_end(json_writer_t * jw)
{
    if (jw_prev(jw) == ',') jw->wptr--;
    jw_put(jw, ']');
}

void jw_kv_start(json_writer_t *jw, const char *key)
{
    jw_write_quoted(jw, key);
    jw_put(jw, ':');
}

void jw_kv_end(json_writer_t *jw)
{
    if ( jw_prev(jw) == ',' ) return;
    jw_put(jw, ',');
}

void jw_str(json_writer_t *jw, const char *value)
{
    jw_write_quoted(jw, value);
    jw_put(jw, ',');
}

void jw_kv_str(json_writer_t *jw, const char *key, const char *value)
{
    jw_kv(jw, key) {
        jw_str(jw, value);
    }
}

void jw_int(json_writer_t *jw, int value)
{
    int len = 0;
    int v = value;
    while(v > 0) {
        len++;
        v /= 10;
    }
    if (value < 0)
        len++;

    jw_can_write(jw, len);
    jw->wptr += sprintf(jw->wptr,"%d", value);
    jw_put(jw, ',');
}

void jw_int32(json_writer_t *jw, int32_t value)
{
    int len = 0;
    int v = value;
    while(v > 0) {
        len++;
        v /= 10;
    }
    if (value < 0)
        len++;

    jw_can_write(jw, len);
    jw->wptr += sprintf(jw->wptr,"%"PRId32, value);
    jw_put(jw, ',');
}

void jw_uint64(json_writer_t *jw, uint64_t value)
{
    int len = 0;
    uint64_t v = value;

    while(v > 0) {
        len++;
        v /= 10;
    }

    jw_can_write(jw, len);
    jw->wptr += sprintf(jw->wptr,"%"PRIu64, value);
    jw_put(jw, ',');
}



void jw_kv_int(json_writer_t *jw, const char *key, int value)
{
    jw_kv(jw, key) {
        jw_int(jw, value);
    }
}
void jw_kv_int32(json_writer_t *jw, const char *key, int32_t value)
{
    jw_kv(jw, key) {
        jw_int32(jw, value);
    }
}
void jw_kv_uint64(json_writer_t *jw, const char *key, uint64_t value)
{
    jw_kv(jw, key) {
        jw_uint64(jw, value);
    }
}

void jw_kv_bool(json_writer_t *jw, const char *key, bool value)
{
    jw_kv(jw, key) {
        jw_format(jw, "%s", value ? "true" : "false");
        jw_put(jw, ',');
    }
}

void jw_kv_ip4(json_writer_t *jw, const char *key, ip4_addr_t ip4)
{
    jw_kv_str(jw, key, inet_ntoa(ip4));
}

void jw_kv_mac_in_dec(json_writer_t *jw, const char *key, const char *mac)
{
    jw_kv(jw, key) {
        jw_format(jw, "\"%d,%d,%d,%d,%d,%d\"", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        jw_put(jw, ',');
    }
}



void jw_format(json_writer_t *jw, const char *format, ...)
{
    va_list args;
    int r;
    size_t size = (jw->buf + jw->len) - jw->wptr;

    va_start (args, format);
    r = vsnprintf (jw->wptr, size, format, args);
    va_end (args);

    if (r > 0 && r < size) {
        jw->wptr += r;
    } else {
        /** force error */
        jw_can_write(jw, jw->len +1);
    }
}

