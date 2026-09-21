/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 */
#include "gridconnect.h"

static char rr_hex_digit(int nibble)
{
    return "0123456789ABCDEF"[nibble & 0xF];
}

static int rr_hex_value(char byte)
{
    if (byte >= '0' && byte <= '9') {
        return byte - '0';
    }
    if (byte >= 'A' && byte <= 'F') {
        return byte - 'A' + 10;
    }
    if (byte >= 'a' && byte <= 'f') {
        return byte - 'a' + 10;
    }
    return -1;
}

void rr_gc_parser_init(rr_gc_parser *parser)
{
    parser->state = 0;
    parser->hex_n = 0;
    parser->data_n = 0;
}

int rr_gc_format(char *out, int out_cap, uint32_t identifier,
                 const uint8_t *data, int data_len)
{
    int pos = 0;
    int i;

    if (out_cap < 14 || data_len < 0 || data_len > 8) {
        return -1;
    }
    if (data_len > 0 && data == 0) {
        return -1;
    }
    out[pos++] = ':';
    out[pos++] = 'X';
    for (i = 7; i >= 0; i--) {
        out[pos++] = rr_hex_digit((int)((identifier >> (i * 4)) & 0xF));
    }
    out[pos++] = 'N';
    for (i = 0; i < data_len; i++) {
        out[pos++] = rr_hex_digit(data[i] >> 4);
        out[pos++] = rr_hex_digit(data[i]);
    }
    if (pos + 1 >= out_cap) {
        return -1;
    }
    out[pos++] = ';';
    out[pos] = '\0';
    return pos;
}

static int rr_gc_finish(rr_gc_parser *parser, uint32_t *identifier,
                        uint8_t *data, int *data_len)
{
    uint32_t id = 0;
    int i;

    if (parser->hex_n != 8 || (parser->data_n % 2) != 0) {
        rr_gc_parser_init(parser);
        return 0;
    }
    for (i = 0; i < 8; i++) {
        int value = rr_hex_value(parser->hex[i]);
        if (value < 0) {
            rr_gc_parser_init(parser);
            return 0;
        }
        id = (id << 4) | (uint32_t)value;
    }
    *identifier = id;
    *data_len = parser->data_n / 2;
    for (i = 0; i < *data_len; i++) {
        data[i] = parser->data[i];
    }
    rr_gc_parser_init(parser);
    return 1;
}

static void rr_gc_store_nibble(rr_gc_parser *parser, int value)
{
    if ((parser->data_n % 2) == 0) {
        parser->data[parser->data_n / 2] = (uint8_t)(value << 4);
    } else {
        parser->data[parser->data_n / 2] |= (uint8_t)value;
    }
    parser->data_n++;
}

static int rr_gc_feed_header(rr_gc_parser *parser, char byte)
{
    if (parser->state == 0) {
        parser->state = (byte == ':') ? 1 : 0;
        return 0;
    }
    if (byte == 'X' || byte == 'x') {
        parser->state = 2;
        parser->hex_n = 0;
        parser->data_n = 0;
        return 0;
    }
    parser->state = (byte == ':') ? 1 : 0;
    return 0;
}

static int rr_gc_feed_id(rr_gc_parser *parser, char byte)
{
    if (parser->hex_n < 8) {
        parser->hex[parser->hex_n++] = byte;
        return 0;
    }
    if (byte != 'N' && byte != 'n') {
        rr_gc_parser_init(parser);
        return 0;
    }
    parser->state = 3;
    return 0;
}

static int rr_gc_feed_data(rr_gc_parser *parser, char byte, uint32_t *identifier,
                           uint8_t *data, int *data_len)
{
    int value;

    if (byte == ';') {
        return rr_gc_finish(parser, identifier, data, data_len);
    }
    if (parser->data_n >= 16) {
        rr_gc_parser_init(parser);
        return 0;
    }
    value = rr_hex_value(byte);
    if (value < 0) {
        rr_gc_parser_init(parser);
        return 0;
    }
    rr_gc_store_nibble(parser, value);
    return 0;
}

int rr_gc_feed(rr_gc_parser *parser, char byte, uint32_t *identifier,
               uint8_t *data, int *data_len)
{
    if (parser->state < 2) {
        return rr_gc_feed_header(parser, byte);
    }
    if (parser->state == 2) {
        return rr_gc_feed_id(parser, byte);
    }
    return rr_gc_feed_data(parser, byte, identifier, data, data_len);
}
