/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * Pico 2 W LCC node 05.01.01.01.A5.05.
 * Wi-Fi station when a gitignored wrap is present. GridConnect TCP 12021.
 * No display and no CAN HAT in this image.
 */
#include "board_pins.h"
#include "gridconnect.h"
#include "lcc_login.h"
#include "lcc_node_services.h"
#include "restouch35.h"

#include "pico/stdio_usb.h"
#include "pico/stdlib.h"

#include "FreeRTOS.h"
#include "task.h"

int rr_run_all_tests(void);
void rr_uart_ring_init(void);
void rr_uart_raw_banner(void);

#include <stdio.h>
#include <string.h>

#if RR_LED_CYW43
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/tcp.h"
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

#if RR_LED_CYW43 && RR_WIFI_WRAP
#include "wifi_psk_wrap.inc"
#include "hub_host.h"
#include "mbedtls/gcm.h"
#include "mbedtls/aes.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/md.h"
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

int64_t mbedtls_ms_time(void)
{
    return 0;
}

void mbedtls_platform_zeroize(void *buf, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)buf;
    while (len-- > 0) {
        *p++ = 0;
    }
}

void mbedtls_zeroize_and_free(void *buf, size_t len)
{
    mbedtls_platform_zeroize(buf, len);
    free(buf);
}

static char g_login[9 * RR_GC_FRAME_MAX];
static int g_login_len;
static char g_ip_text[20];
static char g_wifi_psk[65];
static const char *g_lan_text = "unknown";
static volatile int g_wifi_icon = RR_WIFI_ICON_OFF;
static volatile int g_lcc_icon = RR_SVC_ICON_DIM;

#if RR_PANEL_RES35
static void panel_show(void)
{
    /* JMRI web port 12080 is not probed. That icon stays dim. Touch is not read. */
    rr_restouch_status(g_wifi_icon, g_lcc_icon, RR_SVC_ICON_DIM);
}
#else
static void panel_show(void) {}
#endif

#ifdef DEBUG
#define RR_DBG(...) printf(__VA_ARGS__)
static volatile int g_led_ready;
#else
#define RR_DBG(...) ((void)0)
#endif

static void print_identity(void)
{
    printf("TARGET node 05.01.01.01.A5.05\n");
    printf("TARGET board %s panel %s transport %s\n",
           RR_PICO_BOARD, RR_LCD_PANEL, RR_LCC_TRANSPORT);
    printf("TARGET pin conflicts %d\n", rr_pin_conflict_count());
    printf("OpenLCB Node ID: 05.01.01.01.A5.05\n");
}

#if RR_LED_CYW43
static void print_unique_id(void)
{
    pico_unique_board_id_t id;
    int i;

    pico_get_unique_board_id(&id);
#ifdef DEBUG
    printf("SPI flash unique ID: ");
    for (i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        printf("%02X", id.id[i]);
    }
    printf("\n");
#endif
}

static void print_mac(const uint8_t mac[6])
{
    printf("MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#if RR_WIFI_WRAP
static int unwrap_psk(const uint8_t mac[6], char *psk, int psk_cap)
{
    pico_unique_board_id_t id;
    uint8_t ikm[8 + 6 + 6];
    uint8_t info[5 + 6];
    uint8_t key[32];
    uint8_t plain[64];
    uint8_t hmac_in[12 + 64];
    uint8_t expect[32];
    uint8_t nonce_counter[16];
    uint8_t stream_block[16];
    const uint8_t *blob = kWifiWrapBlob;
    mbedtls_aes_context aes;
    int clen;
    uint64_t node = RR_NODE_ID_U64;
    size_t nc_off = 0;
    int i;

    if (blob[0] != 2 || psk_cap < 65) {
        printf("TARGET unwrap bad hdr %u cap %d\n", blob[0], psk_cap);
        return -1;
    }
    pico_get_unique_board_id(&id);
    memcpy(ikm, id.id, 8);
    memcpy(ikm + 8, mac, 6);
    for (i = 5; i >= 0; i--) {
        ikm[8 + 6 + i] = (uint8_t)(node & 0xFFu);
        node >>= 8;
    }
    info[0] = 0x05;
    info[1] = 0x01;
    info[2] = 0x01;
    info[3] = 0x01;
    info[4] = 0xA5;
    memcpy(info + 5, mac, 6);

#ifdef DEBUG
    printf("TARGET unwrap blob");
    for (i = 0; i < 14; i++) {
        printf(" %02X", blob[i]);
    }
    printf("\n");
#endif
    if (mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                     (const unsigned char *)"owlthree-pico2w-wifi-wrap-v1", 28,
                     ikm, sizeof ikm, info, sizeof info, key, sizeof key) != 0) {
        printf("TARGET unwrap hkdf fail\n");
        return -1;
    }
#ifdef DEBUG
    printf("TARGET unwrap key");
    for (i = 0; i < 8; i++) {
        printf(" %02X", key[i]);
    }
    printf("\n");
#endif
    clen = blob[1 + 12 + 16];
    if (clen <= 0 || clen > 64) {
        printf("TARGET unwrap bad clen %d\n", clen);
        return -1;
    }

    memcpy(hmac_in, blob + 1, 12);
    memcpy(hmac_in + 12, blob + 1 + 12 + 16 + 1, (size_t)clen);
    if (mbedtls_md_hmac(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256),
                        key, 32, hmac_in, 12 + (size_t)clen, expect) != 0 ||
        memcmp(expect, blob + 1 + 12, 16) != 0) {
        printf("TARGET unwrap hmac fail\n");
        return -1;
    }

    memset(nonce_counter, 0, sizeof nonce_counter);
    memcpy(nonce_counter, blob + 1, 12);
    mbedtls_aes_init(&aes);
    if (mbedtls_aes_setkey_enc(&aes, key, 256) != 0 ||
        mbedtls_aes_crypt_ctr(&aes, (size_t)clen, &nc_off, nonce_counter,
                              stream_block, blob + 1 + 12 + 16 + 1,
                              plain) != 0) {
        printf("TARGET unwrap ctr fail\n");
        mbedtls_aes_free(&aes);
        return -1;
    }
    mbedtls_aes_free(&aes);
    printf("TARGET unwrap ctr ok\n");

    memcpy(psk, plain, (size_t)clen);
    psk[clen] = '\0';
    memset(plain, 0, sizeof plain);
    memset(key, 0, sizeof key);
    return clen;
}
#endif

static err_t gc_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    (void)arg;
    (void)pcb;
    (void)len;
    return ERR_OK;
}

static err_t gc_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
    rr_gc_parser parser;
    char reply[512];
    uint32_t identifier = 0;
    uint8_t data[8];
    int data_len = 0;
    int i;
    uint16_t n;

    (void)arg;
    (void)err;
    if (p == NULL) {
        tcp_close(pcb);
        return ERR_OK;
    }
    tcp_recved(pcb, p->tot_len);
    rr_gc_parser_init(&parser);
    for (n = 0; n < p->tot_len; n++) {
        char byte = (char)pbuf_get_at(p, n);
        if (!rr_gc_feed(&parser, byte, &identifier, data, &data_len)) {
            continue;
        }
        i = rr_reply_gridconnect(identifier, data, data_len, RR_ALIAS_A505,
                                 RR_NODE_ID_U64, reply, (int)sizeof reply);
        if (i <= 0) {
            i = rr_service_frame(identifier, data, data_len, RR_ALIAS_A505,
                                 RR_NODE_ID_U64, reply, (int)sizeof reply);
        }
        if (i > 0) {
            tcp_write(pcb, reply, (u16_t)i, TCP_WRITE_FLAG_COPY);
            tcp_output(pcb);
        }
    }
    pbuf_free(p);
    return ERR_OK;
}

static err_t gc_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    (void)arg;
    if (err != ERR_OK || pcb == NULL) {
        printf("TARGET hub connect failed\n");
        g_lcc_icon = RR_SVC_ICON_FAIL;
        return ERR_ABRT;
    }
    tcp_recv(pcb, gc_recv);
    tcp_sent(pcb, gc_sent);
    tcp_write(pcb, g_login, (u16_t)g_login_len, TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
    printf("TARGET hub connected\n");
    g_lcc_icon = RR_SVC_ICON_OK;
    return ERR_OK;
}

static int dial_hub(void)
{
    ip_addr_t addr;
    struct tcp_pcb *pcb = tcp_new_ip_type(IPADDR_TYPE_V4);

    if (g_login_len <= 0) {
        g_login_len = rr_login_gridconnect(RR_ALIAS_A505, RR_NODE_ID_U64,
                                           g_login, (int)sizeof g_login);
    }
    if (pcb == NULL || !ipaddr_aton(RR_HUB_HOST, &addr)) {
        printf("TARGET hub missing\n");
        g_lcc_icon = RR_SVC_ICON_FAIL;
        return -1;
    }
    printf("TARGET hub dial %s %d\n", RR_HUB_HOST, RR_GC_PORT);
    if (tcp_connect(pcb, &addr, RR_GC_PORT, gc_connected) != ERR_OK) {
        printf("TARGET hub connect failed\n");
        g_lcc_icon = RR_SVC_ICON_FAIL;
        tcp_close(pcb);
        return -1;
    }
    return 0;
}
#endif

#if RR_LED_CYW43 && RR_WIFI_WRAP
static void join_and_listen(const uint8_t mac[6])
{
    int pw = unwrap_psk(mac, g_wifi_psk, (int)sizeof g_wifi_psk);

    if (pw < 0) {
        printf("TARGET wifi unwrap failed\n");
        //g_wifi_icon = RR_WIFI_ICON_FAIL;
        //g_lcc_icon = RR_SVC_ICON_FAIL;
        return;
    }
    //g_wifi_icon = RR_WIFI_ICON_SEARCH;
    RR_DBG("TARGET wifi join start\n");
    //printf("TARGET wifi ssid %s\n", kWifiWrapSsid);

    if (cyw43_arch_wifi_connect_timeout_ms(kWifiWrapSsid, g_wifi_psk,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000) != 0) {
        printf("TARGET wifi join failed\n");
        //g_lan_text = "wifi join failed";
        //g_wifi_icon = RR_WIFI_ICON_FAIL;
        //g_lcc_icon = RR_SVC_ICON_FAIL;
        return;
    }
    
    {
        const ip4_addr_t *ip = netif_ip4_addr(netif_default);
        snprintf(g_ip_text, sizeof g_ip_text, "%s", ip4addr_ntoa(ip));
        g_lan_text = (ip4_addr1(ip) == 192 && ip4_addr2(ip) == 168 && ip4_addr3(ip) == 1)
                         ? "192.168.1 same" : "other";
        printf("TARGET ip %s\n", g_ip_text);
        printf("TARGET lan %s\n", g_lan_text);
        g_wifi_icon = RR_WIFI_ICON_OK;
    }
    cyw43_arch_lwip_begin();
    dial_hub();
    cyw43_arch_lwip_end();
}
#endif

#if RR_LED_CYW43
static void start_radio(void)
{
    uint8_t mac[6];

    print_unique_id();
#ifdef DEBUG
    printf("TARGET radio start\n");
#endif
    if (cyw43_arch_init() != 0) {
        printf("TARGET cyw43 init failed\n");
        while (1) {
            tight_loop_contents();
        }
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    RR_DBG("TARGET radio up\n");
    cyw43_arch_enable_sta_mode();
    cyw43_wifi_get_mac(&cyw43_state, CYW43_ITF_STA, mac);
    print_mac(mac);
#if RR_WIFI_WRAP
    g_wifi_icon = RR_WIFI_ICON_SEARCH;
    join_and_listen(mac);
#else
    printf("TARGET wifi secret missing\n");
    g_wifi_icon = RR_WIFI_ICON_FAIL;
    g_lcc_icon = RR_SVC_ICON_FAIL;
#endif
#ifdef DEBUG
    g_led_ready = 1;
#endif
}
#endif

static int checks_ok(void)
{
    print_identity();
#ifdef DEBUG
    printf("TARGET rtos freertos\n");
#endif
#if RR_FIRMWARE_TESTS
    printf("TARGET unity %s\n", rr_run_all_tests() == 0 ? "ok" : "fail");
#endif
#ifdef DEBUG
    printf("TARGET after tests tick %u\n", to_ms_since_boot(get_absolute_time()));
#endif
    g_login_len = rr_login_gridconnect(RR_ALIAS_A505, RR_NODE_ID_U64,
                                       g_login, (int)sizeof g_login);
    if (rr_pin_conflict_count() != 0 || rr_pin_uses_wireless_gpio() != 0 || g_login_len < 0) {
        printf("TARGET pin map rejected\n");
        return 0;
    }
    return 1;
}

static void start_board(void)
{
#if RR_PANEL_RES35
    printf("TARGET lcd enter %s:%d\n", __FILE__, __LINE__);

    rr_restouch_init();
    printf("TARGET lcd leave %s:%d\n", __FILE__, __LINE__);

    g_wifi_icon = RR_WIFI_ICON_SEARCH;

    printf("TARGET lcd enter %s:%d\n", __FILE__, __LINE__);

    panel_show();
    printf("TARGET lcd leave %s:%d\n", __FILE__, __LINE__);

#endif
#if RR_LED_CYW43
    start_radio();
#else
    printf("TARGET wifi absent\n");
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#ifdef DEBUG
    g_led_ready = 1;
#endif
#endif
}

static void blink_led(int led_on)
{
#if RR_LED_CYW43
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_on);
#else
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
#endif
}

static void debug_beat(int led_on)
{
#ifdef DEBUG
    printf("TARGET alive led %s tick %lu\n",
           led_on ? "on" : "off",
           (unsigned long)xTaskGetTickCount());
    printf("TARGET ip %s\n", g_ip_text[0] ? g_ip_text : "none");
    printf("TARGET lan %s\n", g_lan_text);
    print_identity();
#else
    (void)led_on;
#endif
}

static TickType_t g_wifi_next_try;

static void wifi_ensure(void)
{
#if RR_WIFI_WRAP
    TickType_t now = xTaskGetTickCount();

    if (g_ip_text[0] || !g_wifi_psk[0]) {
        return;
    }
    if ((int32_t)(now - g_wifi_next_try) < 0) {
        return;
    }
    g_wifi_next_try = now + pdMS_TO_TICKS(5000);
    printf("TARGET wifi retry\n");
    /* connect 30000, then ip + dial_hub as above */
#endif
}

#if HACK
static void wifi_ensure(void)
{
#if RR_WIFI_WRAP
    if (g_ip_text[0] || !g_wifi_psk[0]) {
        return;
    }
    printf("TARGET wifi retry\n");
    if (cyw43_arch_wifi_connect_timeout_ms(
            kWifiWrapSsid, g_wifi_psk,
            CYW43_AUTH_WPA2_AES_PSK, 30000) != 0) {
        printf("TARGET wifi join failed\n");
        return;
    }
    {
        const ip4_addr_t *ip = netif_ip4_addr(netif_default);
        snprintf(g_ip_text, sizeof g_ip_text, "%s", ip4addr_ntoa(ip));
        printf("TARGET ip %s\n", g_ip_text);
        g_wifi_icon = RR_WIFI_ICON_OK;
    }
    cyw43_arch_lwip_begin();
    dial_hub();
    cyw43_arch_lwip_end();
#endif
}
#endif
static void app_task(void *unused)
{
    (void)unused;
    printf("TARGET task\n");
    start_board();
    if (!checks_ok()) {
        while (1) {
            tight_loop_contents();
        }
    }
    while (1) {
        static int led_on = 0;

#ifdef DEBUG
        printf("TARGET lcd enter %s:%d\n", __FILE__, __LINE__);
#endif
        panel_show();
#ifdef DEBUG
        printf("TARGET lcd leave %s:%d\n", __FILE__, __LINE__);
#endif
        wifi_ensure();

        led_on = !led_on;
        blink_led(led_on);
#ifdef DEBUG        
        printf("TARGET blink/panel %s:%d\n", __FILE__, __LINE__);
#endif
        debug_beat(led_on);
#ifdef DEBUG
        printf("TARGET loop after beat %s:%d\n", __FILE__, __LINE__);
#endif
        vTaskDelay(pdMS_TO_TICKS(500));
#ifdef DEBUG
        printf("TARGET loop after delay %s:%d\n", __FILE__, __LINE__);
#endif
    }
}

#ifdef DEBUG
static void alive_task(void *unused)
{
    unsigned long beat = 0;

    (void)unused;
    while (!g_led_ready) {
        beat++;
#ifdef DEBUG
        printf("TARGET alive %lu led wait-radio tick %lu uart0 GP0\n",
               beat, (unsigned long)xTaskGetTickCount());
#endif
        vTaskDelay(pdMS_TO_TICKS(500));
    }
  vTaskDelete(NULL);
}
#endif

int main(void)
{
    rr_uart_raw_banner();
    stdio_init_all();
    rr_uart_ring_init();
    printf("TARGET boot\n");
    RR_DBG("TARGET debug uart0 115200 GP0-TX GP1-RX\n");
#ifdef DEBUG
    if (xTaskCreate(alive_task, "alive", 1024, 0, 1, 0) != pdPASS) {
        printf("TARGET alive task failed\n");
    }
#endif
    if (xTaskCreate(app_task, "lcc", 8192, 0, 1, 0) != pdPASS) {
        printf("TARGET task create failed\n");
    }
    vTaskStartScheduler();
    while (1) {
        tight_loop_contents();
    }
}
