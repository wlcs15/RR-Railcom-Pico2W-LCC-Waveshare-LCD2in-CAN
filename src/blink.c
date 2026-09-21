/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * USB serial blink. Pico 2 W and Pico W use the wireless-chip LED.
 * Pico 2 and Pico use GPIO 25. This image does not join Wi-Fi.
 */
#include "board_pins.h"

#include "pico/stdlib.h"

#include <stdio.h>

#if RR_LED_CYW43
#include "pico/cyw43_arch.h"
#endif

#ifndef RR_LCC_TRANSPORT
#define RR_LCC_TRANSPORT "WIFI"
#endif
#ifndef RR_LCD_PANEL
#define RR_LCD_PANEL "NONE"
#endif
#ifndef RR_PICO_BOARD
#define RR_PICO_BOARD "unset"
#endif

static void led_init(void)
{
#if RR_LED_CYW43
    if (cyw43_arch_init() != 0) {
        printf("cyw43 init failed\n");
    }
#else
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif
}

static void led_set(int on)
{
#if RR_LED_CYW43
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, on ? 1 : 0);
#endif
}

static void print_banner(void)
{
    printf("node 05.01.01.01.A5.05\n");
    printf("board %s panel %s transport %s\n", RR_PICO_BOARD, RR_LCD_PANEL, RR_LCC_TRANSPORT);
#if RR_LED_CYW43
    printf("led cyw43\n");
#else
    printf("led gpio25\n");
#endif
    printf("pin conflicts %d\n", rr_pin_conflict_count());
}

int main(void)
{
    int on = 0;

    stdio_init_all();
    led_init();
    sleep_ms(300);
    print_banner();
    if (rr_pin_conflict_count() != 0 || rr_pin_uses_wireless_gpio() != 0) {
        printf("pin map rejected\n");
        while (1) {
            tight_loop_contents();
        }
    }
    while (1) {
        on = !on;
        led_set(on);
        printf("blink\n");
        sleep_ms(250);
    }
}
