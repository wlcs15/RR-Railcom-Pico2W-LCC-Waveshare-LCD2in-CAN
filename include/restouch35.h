/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * Pico W + Pico-ResTouch-LCD-3.5 preview. Landscape 480x320.
 * Touch is not read. This header is firmware-only.
 */
#ifndef RR_RESTOUCH35_H
#define RR_RESTOUCH35_H

#define RR_WIFI_ICON_OFF 0
#define RR_WIFI_ICON_SEARCH 1
#define RR_WIFI_ICON_OK 2
#define RR_WIFI_ICON_FAIL 3

#define RR_SVC_ICON_DIM 0
#define RR_SVC_ICON_OK 1
#define RR_SVC_ICON_FAIL 2

void rr_restouch_init(void);
void rr_restouch_status(int wifi, int lcc, int jmri);

#endif
