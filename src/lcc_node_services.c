/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 */
#include "lcc_node_services.h"
#include "gridconnect.h"
#include "lcc_login.h"

#include <stdio.h>
#include <string.h>

static const char kCdi[] =
    "<?xml version=\"1.0\"?>"
    "<cdi xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" "
    "xsi:noNamespaceSchemaLocation=\"http://openlcb.org/schema/cdi/1/3/cdi.xsd\">"
    "<identification><manufacturer>OwlThree</manufacturer><model>Pico2W-A505</model>"
    "<hardwareVersion>Pico2W</hardwareVersion><softwareVersion>0.04</softwareVersion>"
    "</identification><acdi/></cdi>";

static int g_consumer_hits;
static uint8_t g_dg[72];
static int g_dg_len;

const char *rr_cdi_xml(void)
{
    return kCdi;
}

int rr_consumer_hits(void)
{
    return g_consumer_hits;
}

static int rr_append_frame(char *out, int cap, int used, uint32_t identifier,
                           const uint8_t *data, int data_len)
{
    int wrote;
    if (used < 0 || used >= cap) {
        return -1;
    }
    wrote = rr_gc_format(out + used, cap - used, identifier, data, data_len);
    return wrote < 0 ? -1 : used + wrote;
}

static uint32_t rr_std_id(uint16_t mti, uint16_t alias)
{
    return 0x19000000u | ((uint32_t)mti << 12) | alias;
}

static int rr_addressed(char *out, int cap, uint16_t mti, uint16_t alias,
                        uint16_t dest, const uint8_t *body, int body_len)
{
    uint8_t frame[8];
    int used = 0;
    int offset = 0;

    frame[0] = (uint8_t)(dest >> 8);
    frame[1] = (uint8_t)dest;
    if (body_len < 0) {
        return -1;
    }
    while (offset < body_len || body_len == 0) {
        int n = body_len - offset;
        if (n > 6) {
            n = 6;
        }
        if (n > 0) {
            memcpy(frame + 2, body + offset, (size_t)n);
        }
        used = rr_append_frame(out, cap, used, rr_std_id(mti, alias), frame, n + 2);
        if (body_len == 0 || used < 0) {
            return used;
        }
        offset += n;
    }
    return used;
}

static int rr_snip(char *out, int cap, uint16_t alias, uint16_t dest)
{
    static const uint8_t body[] = {
        0x01, 'O', 'w', 'l', 'T', 'h', 'r', 'e', 'e', 0,
        'P', 'i', 'c', 'o', '2', 'W', 0,
        'P', 'i', 'c', 'o', '2', 'W', 0,
        '0', '.', '0', '4', 0,
        0x01, 'A', '5', '.', '0', '5', 0,
        'P', 'i', 'c', 'o', ' ', '2', ' ', 'W', ' ', 'L', 'C', 'C', 0
    };
    return rr_addressed(out, cap, 0x0A08, alias, dest, body, (int)sizeof body);
}

static int rr_pip(char *out, int cap, uint16_t alias, uint16_t dest)
{
    /* Datagram, memory config, events, SNIP, CDI. */
    static const uint8_t body[] = {0x54, 0x18, 0x00, 0x00, 0x00, 0x00};
    return rr_addressed(out, cap, 0x0668, alias, dest, body, (int)sizeof body);
}

static int rr_events(char *out, int cap, uint16_t alias, uint64_t node_id)
{
    uint8_t data[8];
    int i;
    int used;
    uint64_t shift = node_id;
    for (i = 5; i >= 0; i--) {
        data[i] = (uint8_t)(shift & 0xFFu);
        shift >>= 8;
    }
    data[6] = 0;
    data[7] = RR_EVENT_TAIL_PRODUCER;
    used = rr_append_frame(out, cap, 0, rr_std_id(0x0547, alias), data, 8);
    data[7] = RR_EVENT_TAIL_CONSUMER;
    return rr_append_frame(out, cap, used, rr_std_id(0x04C7, alias), data, 8);
}

static int rr_same_event(const uint8_t *data, int data_len, uint64_t node_id, uint8_t tail)
{
    int i;
    if (data_len < 8) {
        return 0;
    }
    for (i = 0; i < 6; i++) {
        if (data[i] != (uint8_t)(node_id >> (8 * (5 - i)))) {
            return 0;
        }
    }
    return data[6] == 0 && data[7] == tail;
}

static int rr_send_datagram(char *out, int cap, int used, uint16_t alias,
                            uint16_t dest, const uint8_t *payload, int len)
{
    int offset = 0;
    while (offset < len) {
        int n = len - offset;
        uint32_t kind = 0x1C000000u;
        if (n > 8) {
            n = 8;
        }
        if (len <= 8) {
            kind = 0x1A000000u;
        } else if (offset == 0) {
            kind = 0x1B000000u;
        } else if (offset + n >= len) {
            kind = 0x1D000000u;
        }
        used = rr_append_frame(out, cap, used,
                               kind | ((uint32_t)dest << 12) | alias,
                               payload + offset, n);
        if (used < 0) {
            return -1;
        }
        offset += n;
    }
    return used;
}

static int rr_cdi_read(char *out, int cap, uint16_t alias, uint16_t dest,
                       const uint8_t *req, int req_len)
{
    uint8_t payload[72];
    uint32_t address;
    int count;
    int cdi_len;
    int n;
    int used;

    if (req_len < 7 || req[0] != 0x20 || req[1] != 0x43) {
        return 0;
    }
    address = ((uint32_t)req[2] << 24) | ((uint32_t)req[3] << 16) |
              ((uint32_t)req[4] << 8) | req[5];
    count = req[6];
    if (count > 64) {
        count = 64;
    }
    cdi_len = (int)strlen(kCdi) + 1;
    payload[0] = 0x20;
    payload[1] = 0x53;
    memcpy(payload + 2, req + 2, 4);
    n = 0;
    if ((int)address < cdi_len) {
        n = cdi_len - (int)address;
        if (n > count) {
            n = count;
        }
        memcpy(payload + 6, kCdi + address, (size_t)n);
    }
    used = rr_addressed(out, cap, 0x0A28, alias, dest, 0, 0);
    return rr_send_datagram(out, cap, used, alias, dest, payload, 6 + n);
}

static int rr_on_datagram(char *out, int cap, uint16_t alias, uint16_t src,
                          const uint8_t *data, int data_len)
{
    int n = rr_cdi_read(out, cap, alias, src, data, data_len);
    if (n != 0) {
        return n;
    }
    return rr_addressed(out, cap, 0x0A48, alias, src, 0, 0);
}

static uint16_t rr_dest_alias(const uint8_t *data, int data_len)
{
    if (data == 0 || data_len < 2) {
        return 0;
    }
    return (uint16_t)((data[0] << 8) | data[1]);
}

static int rr_take_datagram(uint32_t kind, const uint8_t *data, int data_len)
{
    if (kind == 0x02000000u || kind == 0x03000000u) {
        g_dg_len = 0;
    }
    if (g_dg_len + data_len > (int)sizeof g_dg) {
        g_dg_len = 0;
        return 0;
    }
    memcpy(g_dg + g_dg_len, data, (size_t)data_len);
    g_dg_len += data_len;
    if (kind == 0x02000000u || kind == 0x05000000u) {
        return 1;
    }
    return 0;
}

static int rr_name_reply(uint16_t mti, uint16_t dest, uint16_t alias, uint16_t src,
                         char *out, int out_cap)
{
    if (mti == 0x0DE8u && dest == alias) {
        return rr_snip(out, out_cap, alias, src);
    }
    if (mti == 0x0828u && dest == alias) {
        return rr_pip(out, out_cap, alias, src);
    }
    return -2;
}

static int rr_standard(uint32_t identifier, const uint8_t *data, int data_len,
                       uint16_t alias, uint64_t node_id, char *out, int out_cap)
{
    uint16_t mti = (uint16_t)((identifier >> 12) & 0x0FFFu);
    uint16_t src = (uint16_t)(identifier & 0x0FFFu);
    uint16_t dest = rr_dest_alias(data, data_len);
    int named;

    if ((identifier & 0x1F000000u) != 0x19000000u) {
        return -1;
    }
    named = rr_name_reply(mti, dest, alias, src, out, out_cap);
    if (named != -2) {
        return named;
    }
    if (mti == 0x0970u || (mti == 0x0968u && dest == alias)) {
        return rr_events(out, out_cap, alias, node_id);
    }
    if (mti == 0x05B4u && rr_same_event(data, data_len, node_id, RR_EVENT_TAIL_CONSUMER)) {
        g_consumer_hits++;
        printf("TARGET consumer\n");
    }
    return 0;
}

int rr_service_frame(uint32_t identifier, const uint8_t *data, int data_len,
                     uint16_t alias, uint64_t node_id, char *out, int out_cap)
{
    uint32_t top = identifier & 0x1F000000u;
    uint16_t field = (uint16_t)((identifier >> 12) & 0x0FFFu);

    if ((identifier & 0x0FFFu) == alias) {
        return 0;
    }
    if ((identifier & 0x1F000000u) == 0x19000000u) {
        return rr_standard(identifier, data, data_len, alias, node_id, out, out_cap);
    }
    if ((top & 0x18000000u) != 0x18000000u || field != alias) {
        return 0;
    }
    if (!rr_take_datagram(top & 0x07000000u, data, data_len)) {
        return 0;
    }
    return rr_on_datagram(out, out_cap, alias, (uint16_t)(identifier & 0x0FFFu),
                          g_dg, g_dg_len);
}
