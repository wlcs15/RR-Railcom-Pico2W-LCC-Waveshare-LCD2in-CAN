#include <stdio.h>
#include "pico/stdlib.h"
#include "xl2515.h"

uint8_t xl2515_read_reg_byte(uint8_t reg);
extern uint8_t g_stat_after_reset;

#define LED_PIN 25
int main()
{
    uint32_t send_id = 0x123;
    uint8_t data[8] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77};
    bool led_state = false;

    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    xl2515_init(KBPS125);
    while (true) {
        printf("Hello, world!\n");
        printf("TARGET can after reset STAT=%02x\n", g_stat_after_reset);
        xl2515_send(send_id, data, 8);
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        sleep_ms(1000);
    }
}
