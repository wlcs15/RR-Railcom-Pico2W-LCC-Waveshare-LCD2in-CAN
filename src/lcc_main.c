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
#include "pico/stdio.h"
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

#if !RR_LED_CYW43
#include "tusb.h"
#endif

#if !RR_LED_CYW43
#include "xl2515.h"
#endif

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
static volatile int g_jmri_icon = RR_SVC_ICON_DIM;

#if RR_PANEL_RES35
static void panel_show(void)
{
    /* JMRI web port 12080 is not probed. That icon stays dim. Touch is not read. */
    rr_restouch_status(g_wifi_icon, g_lcc_icon, g_jmri_icon);
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
        g_lcc_icon = RR_SVC_ICON_FAIL;
        g_jmri_icon = RR_SVC_ICON_FAIL;
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
        g_jmri_icon = RR_SVC_ICON_FAIL;
        return ERR_ABRT;
    }
    tcp_recv(pcb, gc_recv);
    tcp_sent(pcb, gc_sent);
    tcp_write(pcb, g_login, (u16_t)g_login_len, TCP_WRITE_FLAG_COPY);
    tcp_output(pcb);
    printf("TARGET hub connected\n");
    g_lcc_icon = RR_SVC_ICON_OK;
    g_jmri_icon = RR_SVC_ICON_OK;
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
        g_jmri_icon = RR_SVC_ICON_FAIL;
        return -1;
    }
    printf("TARGET hub dial %s %d\n", RR_HUB_HOST, RR_GC_PORT);
    if (tcp_connect(pcb, &addr, RR_GC_PORT, gc_connected) != ERR_OK) {
        printf("TARGET hub connect failed\n");
        g_lcc_icon = RR_SVC_ICON_FAIL;
        g_jmri_icon = RR_SVC_ICON_FAIL;
        tcp_close(pcb);
        return -1;
    }
    return 0;
}
#endif

#if RR_LED_CYW43 && RR_WIFI_WRAP
static char g_wifi_psk[65];          /* up with g_ip_text, not inside the function */

static void join_and_listen(const uint8_t mac[6])
{
    int pw = unwrap_psk(mac, g_wifi_psk, (int)sizeof g_wifi_psk);

    if (pw < 0) {
        printf("TARGET wifi unwrap failed\n");
        g_wifi_icon = RR_WIFI_ICON_FAIL;
        g_lcc_icon = RR_SVC_ICON_FAIL;
        g_jmri_icon = RR_SVC_ICON_FAIL;
        return;
    }
    //g_wifi_icon = RR_WIFI_ICON_SEARCH;
    RR_DBG("TARGET wifi join start\n");
    //printf("TARGET wifi ssid %s\n", kWifiWrapSsid);

    if (cyw43_arch_wifi_connect_timeout_ms(kWifiWrapSsid, g_wifi_psk,
                                           CYW43_AUTH_WPA2_AES_PSK, 30000) != 0) {
        printf("TARGET wifi join failed\n");
        g_ip_text[0] = '\0';
        //g_lan_text = "wifi join failed";
        g_wifi_icon = RR_WIFI_ICON_FAIL;
        g_lcc_icon = RR_SVC_ICON_FAIL;
        g_jmri_icon = RR_SVC_ICON_FAIL;
        return;
    }
    
    {
        const ip4_addr_t *ip = netif_ip4_addr(netif_default);
        snprintf(g_ip_text, sizeof g_ip_text, "%s", ip4addr_ntoa(ip));
#ifdef HACK
        g_lan_text = (ip4_addr1(ip) == 192 && ip4_addr2(ip) == 168 && ip4_addr3(ip) == 1)
                         ? "192.168.1 same" : "other";
#endif
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
static volatile uint32_t g_can_irq;

static void can_int_isr(uint gpio, uint32_t events)
{
    (void)events;
    if (gpio == RR_CAN_INT_GPIO) {
        g_can_irq++;
    }
}

static void can_irq_attach(void)
{
    gpio_init(RR_CAN_INT_GPIO);
    gpio_set_dir(RR_CAN_INT_GPIO, GPIO_IN);
    gpio_pull_up(RR_CAN_INT_GPIO);
    gpio_set_irq_enabled_with_callback(
        RR_CAN_INT_GPIO, GPIO_IRQ_EDGE_FALL, true, can_int_isr);
}

static volatile int g_can_ready;
static void start_board(void)
{
#if RR_PANEL_RES35
#ifdef DEBUG
    printf("TARGET lcd enter %s:%d\n", __FILE__, __LINE__);
#endif
    rr_restouch_init();
#ifdef DEBUG
    printf("TARGET lcd leave %s:%d\n", __FILE__, __LINE__);
#endif
    g_wifi_icon = RR_WIFI_ICON_SEARCH;

#ifdef DEBUG
    printf("TARGET lcd enter %s:%d\n", __FILE__, __LINE__);
#endif
    panel_show();
#ifdef DEBUG
    printf("TARGET lcd leave %s:%d\n", __FILE__, __LINE__);
#endif
#endif

#if !RR_LED_CYW43
#ifdef HACK
    uint8_t payload[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
#endif

    /* demo init name from step 3 */
    xl2515_init(KBPS125);
#ifdef HACK
    xl2515_write_reg_byte(CANCTRL, REQOP_LOOPBACK | CLKOUT_ENABLED);
#endif
    //CANINTE = RX0IE | RX1IE (0x03);

    printf("TARGET can init\n");
#ifdef HACK
    xl2515_send(0x123, payload, 8); //CLS: Note just for loopback test
#endif
    g_can_ready = 1;
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

// Debug added on 24-Sep-2026 @ 2:43 PM CST to help debug why with physical LCC/CAN wired there are no bytes received
    printf("TARGET can cfg CNF=%02x %02x %02x INTE=%02x RXB0=%02x RXB1=%02x\n",
       xl2515_read_reg_byte(CNF1),
       xl2515_read_reg_byte(CNF2),
       xl2515_read_reg_byte(CNF3),
       xl2515_read_reg_byte(CANINTE),
       xl2515_read_reg_byte(RXB0CTRL),
       xl2515_read_reg_byte(RXB1CTRL));
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

static TickType_t g_hub_next_try;

#if RR_LED_CYW43 && RR_WIFI_WRAP
static void hub_ensure(void)
{
#if RR_WIFI_WRAP
    TickType_t now = xTaskGetTickCount();

    if (!g_ip_text[0]) {
        return;
    }
    if (g_lcc_icon == RR_SVC_ICON_OK) {
        return;
    }
    if ((int32_t)(now - g_hub_next_try) < 0) {
        return;
    }
    g_hub_next_try = now + pdMS_TO_TICKS(5000);
    printf("TARGET hub retry\n");
    cyw43_arch_lwip_begin();
    dial_hub();
    cyw43_arch_lwip_end();
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
    if (cyw43_arch_wifi_connect_timeout_ms(
            kWifiWrapSsid, g_wifi_psk,
            CYW43_AUTH_WPA2_AES_PSK, 30000) != 0) {
        printf("TARGET wifi join failed\n");
        g_wifi_icon = RR_WIFI_ICON_FAIL;
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

#if !RR_LED_CYW43
static void rr_usb_puts(const char *s)
{
   int n = 0;

    while (!tud_cdc_connected() && n < 100) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(50));
        n++;
    }
    if (!tud_cdc_connected()) {
        return;
    }
    tud_cdc_write_str(s);
    tud_cdc_write_flush();
}
#endif
  

static void app_task(void *unused)
{
    (void)unused;
#if !RR_LED_CYW43
    rr_usb_puts("TARGET boot\r\n");
    rr_usb_puts("TARGET task\r\n");
#endif
#ifdef HACK
    printf("TARGET boot\n");
    printf("TARGET task\n");
    printf("TARGET loop tick %lu\n", (unsigned long)xTaskGetTickCount());
    fflush(stdout);
#endif
    start_board();

    printf("TARGET can irq gpio %d\n", 8);

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
#if RR_LED_CYW43 && RR_WIFI_WRAP
        wifi_ensure();
        hub_ensure();
#endif
        led_on = !led_on;
        blink_led(led_on);

#if !RR_LED_CYW43
        printf("TARGET can int %d\n", gpio_get(8));
#endif

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

#if !RR_LED_CYW43

static void usb_task(void *unused)
{
    (void)unused;
    for (;;) {
        tud_task();
#ifdef HACK
        if (tud_cdc_connected()) {
            const char *m = "TARGET cdc tick!\r\n";
            tud_cdc_write(m, 18);
            tud_cdc_write_flush();
        }
#endif
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
#endif

#ifdef HACK
static void can_task(void *unused)
{
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
    uint32_t last_irq = 0;

    (void)unused;
    for (;;) {
        if (g_can_irq != last_irq || gpio_get(RR_CAN_INT_GPIO) == 0) {
            last_irq = g_can_irq;
            while (xl2515_recv(&id, data, &len)) {
                printf("TARGET can rx %08lx %u\n",
                       (unsigned long)id, (unsigned)len);
                xl2515_send(id, data, len);   /* echo */
                printf("TARGET can tx %08lx %u\n",
                       (unsigned long)id, (unsigned)len);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
#endif

static void can_task(void *unused)
{
    uint32_t id;
    uint8_t data[8];
    uint8_t len;

    (void)unused;
    while (!g_can_ready) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    for (;;) {
        while (xl2515_recv(&id, data, &len)) {
            unsigned i;
            printf("TARGET can rx %08lx %u",
                   (unsigned long)id, (unsigned)len);
            for (i = 0; i < len; i++) {
                printf(" %02x", data[i]);
            }
            printf("\n");
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

#ifdef HACK2
static void can_task(void *unused)
{
    uint32_t id;
    uint8_t data[8];
    uint8_t len;

    (void)unused;

    // For internal loopback testing
    uint8_t payload[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
    xl2515_send(0x123, payload, 8);

    for (;;) {
        while (xl2515_recv(&id, data, &len)) {
            unsigned i;

            printf("TARGET can rx %08lx %u",
                   (unsigned long)id, (unsigned)len);
            for (i = 0; i < len && i < 8; i++) {
                printf(" %02x", data[i]);
            }
            printf("\n");

            xl2515_send(id, data, len);
            printf("TARGET can tx %08lx %u\n",
                   (unsigned long)id, (unsigned)len);
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
#endif

int main(void)
{
    rr_uart_raw_banner();
    stdio_init_all();
    stdio_usb_init();
    stdio_set_driver_enabled(&stdio_usb, true);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* USB wait + TARGET boot ... */
#if RR_LED_CYW43
    rr_uart_ring_init(); 
#endif

    printf("TARGET boot\n");
    fflush(stdout);


    //rr_uart_ring_init(); // CLS - HACK - appremenly this caused issues on the RPI2350-CAN
#ifdef HACK
    printf("TARGET boot\n");
    RR_DBG("TARGET debug uart0 115200 GP0-TX GP1-RX\n");
#endif
#ifdef DEBUG
    if (xTaskCreate(alive_task, "alive", 1024, 0, 1, 0) != pdPASS) {
        printf("TARGET alive task failed\n");
    }
#endif
    if (xTaskCreate(app_task, "lcc", 8192, 0, 1, 0) != pdPASS) {
        printf("TARGET lcc task create failed\n");
    }

#if !RR_LED_CYW43
    xTaskCreate(usb_task, "usb", 512, 0, 2, 0);
#endif

    if (xTaskCreate(can_task, "can", 2048, 0, 2, 0) != pdPASS) {
        printf("TARGET can task create failed\n");
    } 
    
    vTaskStartScheduler();
    while (1) {
        tight_loop_contents();
    }
}
