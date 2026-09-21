/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 */
#include "lcc_login.h"
#include "gridconnect.h"

#include <stddef.h>

uint16_t rr_alias_from_node(uint64_t node_id)
{
    uint16_t alias = (uint16_t)(node_id & 0xFFFu);
    if (alias == 0) {
        return 1;
    }
    return alias;
}

static void rr_put_node(uint8_t *data, uint64_t node_id)
{
    int i;
    for (i = 5; i >= 0; i--) {
        data[i] = (uint8_t)(node_id & 0xFFu);
        node_id >>= 8;
    }
}

static int rr_append(char *out, int cap, int used, uint32_t identifier,
                     const uint8_t *data, int data_len)
{
    int wrote;
    if (used < 0 || used >= cap) {
        return -1;
    }
    wrote = rr_gc_format(out + used, cap - used, identifier, data, data_len);
    if (wrote < 0) {
        return -1;
    }
    return used + wrote;
}

int rr_login_gridconnect(uint16_t alias, uint64_t node_id, char *out, int out_cap)
{
    uint8_t node[8];
    uint32_t top = 0x10000000u;
    int used = 0;

    if (alias == 0 || alias > 0x0FFF) {
        return -1;
    }
    rr_put_node(node, node_id);
    node[6] = 0;
    node[7] = RR_EVENT_TAIL_PRODUCER;

    used = rr_append(out, out_cap, used,
                     top | 0x07000000u | (uint32_t)((node_id >> 24) & 0xFFF000u) | alias,
                     0, 0);
    used = rr_append(out, out_cap, used,
                     top | 0x06000000u | (uint32_t)((node_id >> 12) & 0xFFF000u) | alias,
                     0, 0);
    used = rr_append(out, out_cap, used,
                     top | 0x05000000u | (uint32_t)(node_id & 0xFFF000u) | alias,
                     0, 0);
    used = rr_append(out, out_cap, used,
                     top | 0x04000000u | (uint32_t)((node_id << 12) & 0xFFF000u) | alias,
                     0, 0);
    used = rr_append(out, out_cap, used, top | 0x00700000u | alias, 0, 0);
    used = rr_append(out, out_cap, used, top | 0x00701000u | alias, node, 6);
    used = rr_append(out, out_cap, used, 0x19000000u | (0x0100u << 12) | alias, node, 6);
    used = rr_append(out, out_cap, used, 0x19000000u | (0x0547u << 12) | alias, node, 8);
    node[7] = RR_EVENT_TAIL_CONSUMER;
    used = rr_append(out, out_cap, used, 0x19000000u | (0x04C7u << 12) | alias, node, 8);
    return used;
}

int rr_reply_gridconnect(uint32_t identifier, const uint8_t *data, int data_len,
                         uint16_t alias, uint64_t node_id, char *out, int out_cap)
{
    uint32_t mti;
    uint8_t node[6];

    if ((identifier & 0x1F000000u) != 0x19000000u) {
        return 0;
    }
    mti = (identifier >> 12) & 0x0FFFu;
    if (mti == 0x0488u) {
        if (data == 0 || data_len < 2) {
            return 0;
        }
        if (((uint16_t)data[0] << 8 | data[1]) != alias) {
            return 0;
        }
    } else if (mti != 0x0490u) {
        return 0;
    }
    rr_put_node(node, node_id);
    return rr_gc_format(out, out_cap, 0x19000000u | (0x0170u << 12) | alias, node, 6);
}
