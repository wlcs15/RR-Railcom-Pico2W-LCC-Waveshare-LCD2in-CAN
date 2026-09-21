/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * GridConnect ASCII: :X<8 hex id>N<hex data>;
 */
#ifndef RR_GRIDCONNECT_H
#define RR_GRIDCONNECT_H

#include <stdint.h>

#define RR_GC_FRAME_MAX 29

typedef struct rr_gc_parser {
    int state;
    int hex_n;
    int data_n;
    char hex[8];
    uint8_t data[8];
} rr_gc_parser;

void rr_gc_parser_init(rr_gc_parser *parser);
int rr_gc_format(char *out, int out_cap, uint32_t identifier,
                 const uint8_t *data, int data_len);
int rr_gc_feed(rr_gc_parser *parser, char byte, uint32_t *identifier,
               uint8_t *data, int *data_len);

#endif
