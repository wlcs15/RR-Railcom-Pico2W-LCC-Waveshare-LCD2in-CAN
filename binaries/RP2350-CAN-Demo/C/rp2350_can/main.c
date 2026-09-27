#include <stdio.h>
#include "pico/stdlib.h"
#include "xl2515.h"

uint8_t xl2515_read_reg_byte(uint8_t reg);
extern uint8_t g_stat_after_reset;

#define LED_PIN 25
int main()
{
    uint32_t rec_id = 0;
    uint8_t recv_data[8];
    uint8_t recv_len = 0;
    bool led_state = false;
    stdio_init_all();
    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    xl2515_init(KBPS125);
    while (true) {
        printf("TARGET can after reset STAT=%02x now %02x intf %02x int %d\n",
               g_stat_after_reset,
               xl2515_read_reg_byte(CANSTAT),
               xl2515_read_reg_byte(CANINTF),
               gpio_get(8));
        if (xl2515_recv(&rec_id, recv_data, &recv_len)) {
            printf("recv %08lx len %u\n", (unsigned long)rec_id, recv_len);
        }
        led_state = !led_state;
        gpio_put(LED_PIN, led_state);
        sleep_ms(1000);
    }
}
