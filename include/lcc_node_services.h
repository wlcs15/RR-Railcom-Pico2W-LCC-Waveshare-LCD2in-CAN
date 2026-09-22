/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * SNIP, protocol-support, CDI memory reads, and one consumer.
 * The CDI is a const byte array, not a filesystem.
 */
#ifndef RR_LCC_NODE_SERVICES_H
#define RR_LCC_NODE_SERVICES_H

#include <stdint.h>

int rr_service_frame(uint32_t identifier, const uint8_t *data, int data_len,
                     uint16_t alias, uint64_t node_id, char *out, int out_cap);
int rr_consumer_hits(void);
const char *rr_cdi_xml(void);

#endif
