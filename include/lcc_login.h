/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * Login frames use the same CAN identifier packing as OpenLcbCLib
 * (CID, RID, AMD, initialization complete, one producer, one consumer).
 * Host tests call this file. The Pico image sends the same bytes.
 */
#ifndef RR_LCC_LOGIN_H
#define RR_LCC_LOGIN_H

#include <stdint.h>

#define RR_GC_PORT 12021
#define RR_ALIAS_A505 0x0505

/* Producer ...A5.05.00.01 and consumer ...A5.05.00.00. */
#define RR_EVENT_TAIL_PRODUCER 0x01
#define RR_EVENT_TAIL_CONSUMER 0x00

uint16_t rr_alias_from_node(uint64_t node_id);
int rr_login_gridconnect(uint16_t alias, uint64_t node_id, char *out, int out_cap);
int rr_reply_gridconnect(uint32_t identifier, const uint8_t *data, int data_len,
                         uint16_t alias, uint64_t node_id, char *out, int out_cap);

#endif
