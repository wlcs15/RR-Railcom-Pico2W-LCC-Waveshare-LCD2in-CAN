/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 */
#include "board_pins.h"

static const int k_assigned[] = {
    RR_LCD_DC_GPIO,    RR_LCD_CS_GPIO,   RR_LCD_SCK_GPIO,  RR_LCD_MOSI_GPIO,
    RR_LCD_RST_GPIO,   RR_LCD_BL_GPIO,   RR_KEY2_GPIO,     RR_KEY3_GPIO,
    RR_KEY0_GPIO,      RR_KEY1_GPIO,     RR_CAN_MISO_GPIO, RR_CAN_CS_GPIO,
    RR_CAN_SCK_GPIO,   RR_CAN_MOSI_GPIO, RR_CAN_INT_GPIO,  RR_CAN_CS1_GPIO,
    RR_CAN_INT2_GPIO,
};

static int wireless_gpio(int gpio)
{
    if (gpio == RR_WIFI_ON_GPIO || gpio == RR_WIFI_DATA_GPIO) {
        return 1;
    }
    if (gpio == RR_WIFI_CS_GPIO || gpio == RR_WIFI_CLK_GPIO) {
        return 1;
    }
    return 0;
}

int rr_pin_assigned_count(void)
{
    return (int)(sizeof k_assigned / sizeof k_assigned[0]);
}

int rr_pin_at(int index)
{
    if (index < 0 || index >= rr_pin_assigned_count()) {
        return -1;
    }
    return k_assigned[index];
}

int rr_pin_conflict_count(void)
{
    int conflicts = 0;
    int count = rr_pin_assigned_count();
    int i;

    for (i = 0; i < count; i++) {
        int j;

        for (j = i + 1; j < count; j++) {
            if (k_assigned[i] == k_assigned[j]) {
                conflicts++;
            }
        }
    }
    return conflicts;
}

int rr_pin_uses_wireless_gpio(void)
{
    int count = rr_pin_assigned_count();
    int i;

    for (i = 0; i < count; i++) {
        if (wireless_gpio(k_assigned[i])) {
            return 1;
        }
    }
    return 0;
}
