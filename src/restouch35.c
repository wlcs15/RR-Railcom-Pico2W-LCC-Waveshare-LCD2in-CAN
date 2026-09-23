/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * ILI9488 status icons for the 3.5 inch preview, laid out like the
 * A5.01 480-wide screen. Touch chip-select stays high.
 */
#include "restouch35.h"

#include "hardware/spi.h"
#include "pico/stdlib.h"
#include <stdio.h>

#define RR_LCD_RST 15
#define RR_LCD_DC 8
#define RR_LCD_CS 9
#define RR_LCD_SCK 10
#define RR_LCD_MOSI 11
#define RR_LCD_MISO 12
#define RR_LCD_BL 13
#define RR_TP_CS 16
#define RR_SD_CS 22

#define RR_LCD_W 480
#define RR_LCD_H 320

static void rr_gpio_out(int pin, int level)
{
    gpio_init((uint)pin);
    gpio_set_dir((uint)pin, GPIO_OUT);
    gpio_put((uint)pin, level);
}

static void rr_cmd(uint8_t value)
{
    uint8_t byte = value;

    gpio_put(RR_LCD_DC, 0);
    gpio_put(RR_LCD_CS, 0);
    spi_write_blocking(spi1, &byte, 1);
    gpio_put(RR_LCD_CS, 1);
}

static void rr_data(uint8_t value)
{
    uint8_t byte = value;

    gpio_put(RR_LCD_DC, 1);
    gpio_put(RR_LCD_CS, 0);
    spi_write_blocking(spi1, &byte, 1);
    gpio_put(RR_LCD_CS, 1);
}

static void rr_cmd_bytes(uint8_t command, const uint8_t *data, int count)
{
    int i;

    rr_cmd(command);
    for (i = 0; i < count; i++) {
        rr_data(data[i]);
    }
}

static void rr_window(int x, int y, int w, int h)
{
    uint8_t col[4];
    uint8_t row[4];
    int x1 = x + w - 1;
    int y1 = y + h - 1;

    col[0] = (uint8_t)(x >> 8);
    col[1] = (uint8_t)x;
    col[2] = (uint8_t)(x1 >> 8);
    col[3] = (uint8_t)x1;
    row[0] = (uint8_t)(y >> 8);
    row[1] = (uint8_t)y;
    row[2] = (uint8_t)(y1 >> 8);
    row[3] = (uint8_t)y1;
    rr_cmd_bytes(0x2A, col, 4);
    rr_cmd_bytes(0x2B, row, 4);
    rr_cmd(0x2C);
}

static void rr_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    uint8_t pair[2];
    int n = w * h;

    if (w <= 0 || h <= 0) {
        return;
    }
    pair[0] = (uint8_t)(color >> 8);
    pair[1] = (uint8_t)color;
    rr_window(x, y, w, h);
    gpio_put(RR_LCD_DC, 1);
    gpio_put(RR_LCD_CS, 0);
    {
        uint8_t chunk[256];
        int i;

        for (i = 0; i < 128; i++) {
            chunk[i * 2] = pair[0];
            chunk[i * 2 + 1] = pair[1];
        }
        while (n > 0) {
            int pixels = n > 128 ? 128 : n;
            spi_write_blocking(spi1, chunk, (size_t)(pixels * 2));
            n -= pixels;
        }
    }
    gpio_put(RR_LCD_CS, 1);
}

static void rr_panel_on(void)
{
    static const uint8_t c2[] = {0x33};
    static const uint8_t c5[] = {0x00, 0x1e, 0x80};
    static const uint8_t b1[] = {0xB0};
    static const uint8_t mad[] = {0x28};
    static const uint8_t e0[] = {
        0x00, 0x13, 0x18, 0x04, 0x0F, 0x06, 0x3a, 0x56,
        0x4d, 0x03, 0x0a, 0x06, 0x30, 0x3e, 0x0f};
    static const uint8_t e1[] = {
        0x00, 0x13, 0x18, 0x01, 0x11, 0x06, 0x38, 0x34,
        0x4d, 0x06, 0x0d, 0x0b, 0x31, 0x37, 0x0f};
    static const uint8_t pix[] = {0x55};

    gpio_put(RR_LCD_RST, 1);
    sleep_ms(50);
    gpio_put(RR_LCD_RST, 0);
    sleep_ms(50);
    gpio_put(RR_LCD_RST, 1);
    sleep_ms(150);

    rr_cmd(0x21);
    rr_cmd_bytes(0xC2, c2, 1);
    rr_cmd_bytes(0xC5, c5, 3);
    rr_cmd_bytes(0xB1, b1, 1);
    rr_cmd_bytes(0x36, mad, 1);
    rr_cmd_bytes(0xE0, e0, 15);
    rr_cmd_bytes(0xE1, e1, 15);
    rr_cmd_bytes(0x3A, pix, 1);
    rr_cmd(0x11);
    sleep_ms(120);
    rr_cmd(0x29);
}

/* 5-wide glyphs, bit 4 is the left column. Only the status labels. */
static const uint8_t k_glyph_c[7] = {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
static const uint8_t k_glyph_i[7] = {0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t k_glyph_j[7] = {0x02, 0x00, 0x06, 0x02, 0x02, 0x12, 0x0C};
static const uint8_t k_glyph_l[7] = {0x0C, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
static const uint8_t k_glyph_m[7] = {0x00, 0x00, 0x1A, 0x15, 0x15, 0x11, 0x11};
static const uint8_t k_glyph_r[7] = {0x00, 0x00, 0x1E, 0x10, 0x10, 0x10, 0x10};

static const uint8_t *rr_glyph(char ch)
{
    if (ch == 'C') {
        return k_glyph_c;
    }
    if (ch == 'I') {
        return k_glyph_i;
    }
    if (ch == 'J') {
        return k_glyph_j;
    }
    if (ch == 'L') {
        return k_glyph_l;
    }
    if (ch == 'M') {
        return k_glyph_m;
    }
    if (ch == 'R') {
        return k_glyph_r;
    }
    return 0;
}

static void rr_draw_text(int x, int y, const char *text, uint16_t fg, uint16_t bg)
{
    int i;

    for (i = 0; text[i] != '\0'; i++) {
        const uint8_t *glyph = rr_glyph(text[i]);
        int row;

        for (row = 0; glyph != 0 && row < 7; row++) {
            int col;

            for (col = 0; col < 5; col++) {
                uint16_t color = (glyph[row] & (0x10 >> col)) ? fg : bg;
                rr_fill_rect(x + col, y + row, 1, 1, color);
            }
        }
        x += 6;
    }
}

static void rr_slash(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = x1 - x0;
    int dy = y1 - y0;
    int step = (dx > dy) ? dx : dy;
    int i;

    if (dx < 0) {
        dx = -dx;
    }
    if (dy < 0) {
        dy = -dy;
    }
    for (i = 0; i <= step; i++) {
        int x = x0 + (x1 - x0) * i / step;
        int y = y0 + (y1 - y0) * i / step;
        rr_fill_rect(x, y, 2, 2, color);
    }
}

static uint16_t rr_svc_color(int state)
{
    if (state == RR_SVC_ICON_OK) {
        return 0x07E0;
    }
    if (state == RR_SVC_ICON_FAIL) {
        return 0xF800;
    }
    return 0x8410;
}

static void rr_draw_wifi(int state)
{
    const int x0 = RR_LCD_W - 36;
    const int y0 = 6;
    uint16_t color = 0x4208;

    if (state == RR_WIFI_ICON_SEARCH) {
        color = 0xFFE0;
    } else if (state == RR_WIFI_ICON_OK) {
        color = 0x07E0;
    } else if (state == RR_WIFI_ICON_FAIL) {
        color = 0xF800;
    }
    rr_fill_rect(x0, y0, 32, 28, 0x0000);
    if (state == RR_WIFI_ICON_OFF) {
        rr_fill_rect(x0 + 14, y0 + 22, 4, 4, 0x4208);
        return;
    }
    rr_fill_rect(x0 + 4, y0 + 20, 6, 6, color);
    if (state == RR_WIFI_ICON_OK) {
        rr_fill_rect(x0 + 13, y0 + 12, 6, 14, color);
        rr_fill_rect(x0 + 22, y0 + 4, 6, 22, color);
    } else if (state == RR_WIFI_ICON_SEARCH) {
        rr_fill_rect(x0 + 13, y0 + 12, 6, 14, 0x4208);
        rr_fill_rect(x0 + 22, y0 + 4, 6, 22, 0x4208);
    }
    if (state == RR_WIFI_ICON_FAIL) {
        rr_slash(x0 + 2, y0 + 2, x0 + 29, y0 + 25, 0xF800);
    }
}

static void rr_draw_svc(int x0, const char *label, int text_x, int state)
{
    const int y0 = 6;
    uint16_t fg = rr_svc_color(state);

    rr_fill_rect(x0, y0, 36, 28, 0x0000);
    rr_draw_text(text_x, y0 + 10, label, fg, 0x0000);
    if (state == RR_SVC_ICON_FAIL) {
        rr_slash(x0 + 2, y0 + 2, x0 + 33, y0 + 25, 0xF800);
    }
}

void rr_restouch_init(void)
{
    rr_gpio_out(RR_TP_CS, 1);
    rr_gpio_out(RR_SD_CS, 1);
    rr_gpio_out(RR_LCD_CS, 1);
    rr_gpio_out(RR_LCD_DC, 1);
    rr_gpio_out(RR_LCD_RST, 1);
    rr_gpio_out(RR_LCD_BL, 1);
    spi_init(spi1, 4000000);
    spi_set_format(spi1, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST); // CLS: Recommended by Grok
    gpio_set_function(RR_LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(RR_LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(RR_LCD_MISO, GPIO_FUNC_SPI);
    
    printf("TARGET lcd rst %d bl %d spi1\n", RR_LCD_RST, RR_LCD_BL);
    rr_panel_on();
#ifdef DEBUG
    printf("TARGET lcd panel_on done\n");
#endif
    rr_fill_rect(0, 0, RR_LCD_W, RR_LCD_H, 0x0010);
    rr_fill_rect(0, 40, RR_LCD_W, 36, 0xFFE0);
    rr_draw_text(8, 50, "A505", 0x0000, 0xFFE0);

    //rr_fill_rect(0, 0, RR_LCD_W, RR_LCD_H, 0xF800);
#ifdef DEBUG
    printf("TARGET lcd fill done 2\n");
#endif
}

void rr_restouch_status(int wifi, int lcc, int jmri)
{
    const int wifi_x = RR_LCD_W - 36;
    const int jmri_x = wifi_x - 6 - 36;
    const int lcc_x = jmri_x - 6 - 36;

    rr_draw_wifi(wifi);
    rr_draw_svc(lcc_x, "LCC", lcc_x + 6, lcc);
    rr_draw_svc(jmri_x, "JMRI", jmri_x + 2, jmri);
}
