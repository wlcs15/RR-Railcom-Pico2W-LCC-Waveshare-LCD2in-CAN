/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * Pin map for Pico 2 W + Pico-LCD-2 (SPI1) + Pico-CAN-B (SPI0).
 * This header does not include the Pico SDK, so host tests can use it.
 */
#ifndef RR_BOARD_PINS_H
#define RR_BOARD_PINS_H

#define RR_NODE_ID_U64 0x05010101A505ULL

#define RR_LCD_DC_GPIO 8
#define RR_LCD_CS_GPIO 9
#define RR_LCD_SCK_GPIO 10
#define RR_LCD_MOSI_GPIO 11
#define RR_LCD_RST_GPIO 12
#define RR_LCD_BL_GPIO 13

#define RR_KEY2_GPIO 2
#define RR_KEY3_GPIO 3
#ifndef RR_KEY0_GPIO
#define RR_KEY0_GPIO 15
#endif
#define RR_KEY1_GPIO 17

#define RR_CAN_MISO_GPIO 4
#define RR_CAN_CS_GPIO 5
#define RR_CAN_SCK_GPIO 6
#define RR_CAN_MOSI_GPIO 7
#define RR_CAN_INT_GPIO 21

/* Optional second CAN-B channel. Unused until that schematic is checked. */
#define RR_CAN_CS1_GPIO 19
#define RR_CAN_INT2_GPIO 22

#define RR_WIFI_ON_GPIO 23
#define RR_WIFI_DATA_GPIO 24
#define RR_WIFI_CS_GPIO 25
#define RR_WIFI_CLK_GPIO 29

_Static_assert(RR_LCD_SCK_GPIO != RR_CAN_SCK_GPIO, "LCD and CAN clocks must differ");
_Static_assert(RR_LCD_CS_GPIO != RR_CAN_CS_GPIO, "LCD and CAN chip-selects must differ");
_Static_assert(RR_LCD_MOSI_GPIO != RR_CAN_MOSI_GPIO, "LCD and CAN MOSI must differ");
_Static_assert(RR_CAN_INT_GPIO != RR_KEY0_GPIO, "CAN interrupt and KEY0 must differ");
_Static_assert(RR_NODE_ID_U64 == 0x05010101A505ULL, "node id must stay 05.01.01.01.A5.05");

int rr_pin_assigned_count(void);
int rr_pin_at(int index);
int rr_pin_conflict_count(void);
int rr_pin_uses_wireless_gpio(void);

#endif
