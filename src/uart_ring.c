/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 *
 * UART0 on GP0 (TX) and GP1 (RX). The PL011 FIFO is 32 bytes.
 * These 512-byte rings are filled and drained from the UART IRQ so
 * printf does not wait on the wire.
 */
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/uart.h"
#include "pico/stdio/driver.h"
#include "pico/stdlib.h"

#include <stdio.h>

#define RR_UART_RING 512
#define RR_UART_BAUD 115200

static uint8_t tx_ring[RR_UART_RING];
static uint8_t rx_ring[RR_UART_RING];
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static volatile uint16_t rx_head;
static volatile uint16_t rx_tail;

static uint16_t ring_next(uint16_t index)
{
    index++;
    if (index >= RR_UART_RING) {
        index = 0;
    }
    return index;
}

static void drain_tx_fifo(void)
{
    while (uart_is_writable(uart0)) {
        uint16_t tail = tx_tail;
        if (tail == tx_head) {
            uart_set_irq_enables(uart0, true, false);
            return;
        }
        uart_get_hw(uart0)->dr = tx_ring[tail];
        tx_tail = ring_next(tail);
    }
}

static void fill_rx_ring(void)
{
    while (uart_is_readable(uart0)) {
        uint8_t byte = (uint8_t)uart_get_hw(uart0)->dr;
        uint16_t next = ring_next(rx_head);
        if (next == rx_tail) {
            continue;
        }
        rx_ring[rx_head] = byte;
        rx_head = next;
    }
}

static void uart_ring_irq(void)
{
    fill_rx_ring();
    drain_tx_fifo();
}

static void uart_ring_out(const char *buf, int length)
{
    int i;
    for (i = 0; i < length; i++) {
        uint32_t save = save_and_disable_interrupts();
        uint16_t next = ring_next(tx_head);
        if (next != tx_tail) {
            tx_ring[tx_head] = (uint8_t)buf[i];
            tx_head = next;
            uart_set_irq_enables(uart0, true, true);
            drain_tx_fifo();
        }
        restore_interrupts(save);
    }
}

static void uart_ring_flush(void)
{
    uint32_t spins = 0;
    while (tx_head != tx_tail && spins < 1000000u) {
        spins++;
        tight_loop_contents();
    }
}

static int uart_ring_in(char *buf, int length)
{
    int count = 0;
    while (count < length && rx_tail != rx_head) {
        uint32_t save = save_and_disable_interrupts();
        buf[count] = (char)rx_ring[rx_tail];
        rx_tail = ring_next(rx_tail);
        restore_interrupts(save);
        count++;
    }
    return count ? count : PICO_ERROR_NO_DATA;
}

static stdio_driver_t uart_ring_driver = {
    .out_chars = uart_ring_out,
    .out_flush = uart_ring_flush,
    .in_chars = uart_ring_in,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = true,
#endif
};

void rr_uart_raw_banner(void)
{
    gpio_set_function(0, UART_FUNCSEL_NUM(uart0, 0));
    gpio_set_function(1, UART_FUNCSEL_NUM(uart0, 1));
    uart_init(uart0, RR_UART_BAUD);
    uart_puts(uart0, "TARGET uart raw GP0\r\n");
}

void rr_uart_ring_init(void)
{
    gpio_set_function(0, UART_FUNCSEL_NUM(uart0, 0));
    gpio_set_function(1, UART_FUNCSEL_NUM(uart0, 1));
    uart_init(uart0, RR_UART_BAUD);
    irq_set_exclusive_handler(UART0_IRQ, uart_ring_irq);
    irq_set_enabled(UART0_IRQ, true);
    uart_set_irq_enables(uart0, true, false);
    stdio_set_driver_enabled(&uart_ring_driver, true);
}
