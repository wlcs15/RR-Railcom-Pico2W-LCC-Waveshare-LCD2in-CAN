#include "xl2515.h"
#include "hardware/spi.h"
#include <string.h>

#define XL2515_SPI_PORT spi1
#define XL2515_SCLK_PIN 10
#define XL2515_MOSI_PIN 11
#define XL2515_MISO_PIN 12
#define XL2515_CS_PIN 9
#define XL2515_INT_PIN 8

uint32_t filter_id = 0x123;
bool g_xl2515_recv_flag = false;

//void xl2515_write_reg_byte(uint8_t reg, uint8_t byte);

static void xl2515_write_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t buf[len + 2];
    buf[0] = CAN_WRITE;
    buf[1] = reg;
    memcpy(buf + 2, data, len);
    gpio_put(XL2515_CS_PIN, 0);
    spi_write_blocking(XL2515_SPI_PORT, buf, len + 2);
    gpio_put(XL2515_CS_PIN, 1);
}

static void xl2515_read_reg(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t buf[2];
    buf[0] = CAN_READ;
    buf[1] = reg;
    gpio_put(XL2515_CS_PIN, 0);
    spi_write_blocking(XL2515_SPI_PORT, buf, 2);
    spi_read_blocking(XL2515_SPI_PORT, 0, data, len);
    gpio_put(XL2515_CS_PIN, 1);
}
#ifdef HACK
static 
#endif
void xl2515_write_reg_byte(uint8_t reg, uint8_t byte)
{
    uint8_t cmd = CAN_WRITE;
    gpio_put(XL2515_CS_PIN, 0);
    spi_write_blocking(XL2515_SPI_PORT, &cmd, 1);
    spi_write_blocking(XL2515_SPI_PORT, &reg, 1);
    spi_write_blocking(XL2515_SPI_PORT, &byte, 1);
    gpio_put(XL2515_CS_PIN, 1);
}

#ifdef HACK
static 
#endif
uint8_t xl2515_read_reg_byte(uint8_t reg)
{
    uint8_t cmd = CAN_READ;
    uint8_t data = 0;
    gpio_put(XL2515_CS_PIN, 0);
    spi_write_blocking(XL2515_SPI_PORT, &cmd, 1);
    spi_write_blocking(XL2515_SPI_PORT, &reg, 1);
    // spi_write_blocking(XL2515_SPI_PORT, &byte, 1);
    spi_read_blocking(XL2515_SPI_PORT, 0, &data, 1);
    gpio_put(XL2515_CS_PIN, 1);
    return data;
}

void xl2515_reset(void)
{
    uint8_t buf = CAN_RESET;
    gpio_put(XL2515_CS_PIN, 0);
    spi_write_blocking(XL2515_SPI_PORT, &buf, 1);
    gpio_put(XL2515_CS_PIN, 1);
}

void gpio_callback(uint gpio, uint32_t events)
{
    if (events & GPIO_IRQ_EDGE_FALL)
    {
        // printf("xl2515 recv data done!\r\n");
        g_xl2515_recv_flag = true;
    }
}

void xl2515_init(xl2515_rate_kbps_t rate_kbps)
{
    uint8_t can_rate_arr[10][3] = {
        {0xBF, 0xFF, 0x87},
        {0x5F, 0xFF, 0x87},
        {0x18, 0XA4, 0x04},
        {0x09, 0XA4, 0x04},
        {0x04, 0x9E, 0x03},
        {0x03, 0x9E, 0x03},
        {0x01, 0x1E, 0x03},
        {0x00, 0x9E, 0x03},
        {0x00, 0x92, 0x02},
        {0x00, 0x82, 0x02}};

    spi_init(XL2515_SPI_PORT, 10 * 1000 * 1000);
    gpio_set_function(XL2515_SCLK_PIN, GPIO_FUNC_SPI);
    gpio_set_function(XL2515_MOSI_PIN, GPIO_FUNC_SPI);
    gpio_set_function(XL2515_MISO_PIN, GPIO_FUNC_SPI);

    gpio_init(XL2515_CS_PIN);
    gpio_set_dir(XL2515_CS_PIN, GPIO_OUT);
    gpio_put(XL2515_CS_PIN, 1);   /* idle high — required */

    printf("TARGET can cs idle %d pin %d\n",
           gpio_get(XL2515_CS_PIN), XL2515_CS_PIN);

    gpio_init(XL2515_INT_PIN);
    gpio_set_dir(XL2515_INT_PIN, GPIO_IN);
    gpio_pull_up(XL2515_INT_PIN);


    xl2515_reset();
    printf("TARGET can after reset STAT=%02x CTRL=%02x CS=%d\n",
           xl2515_read_reg_byte(CANSTAT),
           xl2515_read_reg_byte(CANCTRL),
           gpio_get(XL2515_CS_PIN));
    sleep_ms(100);
    printf("TARGET can after reset and delay STAT=%02x CTRL=%02x CS=%d\n",
           xl2515_read_reg_byte(CANSTAT),
           xl2515_read_reg_byte(CANCTRL),
           gpio_get(XL2515_CS_PIN));

//#ifdef HACK
    gpio_set_irq_enabled_with_callback(XL2515_INT_PIN, GPIO_IRQ_EDGE_FALL, true, gpio_callback); // Commented out for first bringup on hardware, was | GPIO_IRQ_EDGE_RISE
//#endif

    //xl2515_reset();
    //sleep_ms(100);

    // #set baud rate 125Kbps
    // #<7:6>SJW=00(1TQ)
    // #<5:0>BRP=0x03(TQ=[2*(BRP+1)]/Fsoc=2*4/8M=1us)
    // #<5:0>BRP=0x03 (TQ=[2*(BRP+1)]/Fsoc=2*8/16M=1us)
    
    xl2515_write_reg_byte(CNF1, can_rate_arr[rate_kbps][0]);
    xl2515_write_reg_byte(CNF2, can_rate_arr[rate_kbps][1]);
    xl2515_write_reg_byte(CNF3, can_rate_arr[rate_kbps][2]);

    printf("TARGET can after CAN Writes STAT=%02x CTRL=%02x CS=%d\n",
           xl2515_read_reg_byte(CANSTAT),
           xl2515_read_reg_byte(CANCTRL),
           gpio_get(XL2515_CS_PIN));


    // #set TXB0,TXB1
    // #<15:5> SID 11bit canid
    // #<BIT3> exide,1:extended 0:standard
    // xl2515_write_reg(TXB0SIDH, (uint8_t[]){0xFF}, 1);
    // xl2515_write_reg(TXB0SIDL, (uint8_t[]){0xE0}, 1);
    // xl2515_write_reg(TXB0DLC, (uint8_t[]){0x40 | DLC_8}, 1);
    xl2515_write_reg_byte(TXB0SIDH, 0xFF);
    xl2515_write_reg_byte(TXB0SIDL, 0xE0);
    xl2515_write_reg_byte(TXB0DLC, 0x40 | DLC_8);

#ifdef HACK // Grok wanted this replaced with the just below alternate
    // #Set RX
    xl2515_write_reg_byte(RXB0SIDH, 0x00);
    xl2515_write_reg_byte(RXB0SIDL, 0x60);
    xl2515_write_reg_byte(RXB0CTRL, 0x60); // To receive all IDs, changed this to 0x60, was 0x00 for first bring up before enable of interrupts
    xl2515_write_reg_byte(RXB0DLC, DLC_8);

    xl2515_write_reg_byte(RXF0SIDH, (filter_id >> 3) & 0XFF);
    xl2515_write_reg_byte(RXF0SIDL, (filter_id & 0x07) << 5);
    xl2515_write_reg_byte(RXM0SIDH, 0xFF);
    xl2515_write_reg_byte(RXM0SIDL, 0xE0);

    // #can int
    xl2515_write_reg_byte(CANINTF, 0x00); // clean interrupt flag
    xl2515_write_reg_byte(CANINTE, 0x01); // Receive Buffer 0 Full Interrupt Enable Bit
#else
// Grok wants code inserted here on 24-Sep-2026 @ 2:57 PM CST 
    xl2515_write_reg_byte(RXB0SIDH, 0x00);
    xl2515_write_reg_byte(RXB0SIDL, 0x60);
    xl2515_write_reg_byte(RXB0CTRL, 0x64); /* RXM=11, BUKT */
    xl2515_write_reg_byte(RXB0DLC, DLC_8);

    xl2515_write_reg_byte(RXB1CTRL, 0x60);

    xl2515_write_reg_byte(RXM0SIDH, 0x00);
    xl2515_write_reg_byte(RXM0SIDL, 0x00);
    xl2515_write_reg_byte(RXM1SIDH, 0x00);
    xl2515_write_reg_byte(RXM1SIDL, 0x00);

    xl2515_write_reg_byte(CANINTF, 0x00);
    xl2515_write_reg_byte(CANINTE, 0x03);
#endif


    xl2515_write_reg_byte(CANCTRL, REQOP_LISTEN | CLKOUT_ENABLED); //CLS: ToDo - Was REQOP_NORMAL | CLKOUT_ENABLED 
    uint8_t dummy = xl2515_read_reg_byte(CANSTAT);
    if ((dummy & 0xe0) != OPMODE_LISTEN)
    {
        printf("!OPMODE_LISTEN\r\n");
        xl2515_write_reg_byte(CANCTRL, REQOP_LISTEN | CLKOUT_ENABLED); // #set normal mode //CLS: ToDo - Was REQOP_NORMAL | CLKOUT_ENABLED
    }

// Added for debugging why the wired CAN bus was not working on 24-Sep-2026 @ 2:42 PM CST
   xl2515_write_reg_byte(CANINTF, 0x00);
   xl2515_write_reg_byte(CANINTE, 0x03);
}

void xl2515_send(uint32_t can_id, uint8_t *data, uint8_t len)
{
    uint8_t dly = 0;
    while ((xl2515_read_reg_byte(TXB0CTRL) & 0x08) && (dly < 50))
    {
        sleep_ms(1);
        dly++;
    }

    xl2515_write_reg_byte(TXB0SIDH, (can_id >> 3) & 0XFF);
    xl2515_write_reg_byte(TXB0SIDL, (can_id & 0x07) << 5);

    xl2515_write_reg_byte(TXB0EID8, 0);
    xl2515_write_reg_byte(TXB0EID0, 0);
    xl2515_write_reg_byte(TXB0DLC, len);

    xl2515_write_reg(TXB0D0, data, len);
    // for (uint8_t j = 0; j < len; j++) {
    //     xl2515_write_reg_byte(TXB0D0 + j, data[j]);
    // }
    xl2515_write_reg_byte(TXB0CTRL, 0x08);
}

#ifdef HACK
bool xl2515_recv(uint32_t *can_id, uint8_t *data, uint8_t *len)
{
    if (g_xl2515_recv_flag == false)
    {
        return false;
    }
    g_xl2515_recv_flag = false;

    uint16_t sid_h = xl2515_read_reg_byte(RXB0SIDH);
    uint16_t sid_l = xl2515_read_reg_byte(RXB0SIDL);
    *can_id = (sid_h << 3) | (sid_l >> 5);

    while (1)
    {
        if (xl2515_read_reg_byte(CANINTF) & 0x01)
        {
            *len = xl2515_read_reg_byte(RXB0DLC);
            // printf("len = %d\r\n", len);
            for (uint8_t i = 0; i < *len; i++)
            {
                data[i] = xl2515_read_reg_byte(RXB0D0 + i);
                // printf("rx buf =%d\r\n",CAN_RX_Buf[i]);
            }
            break;
        }
    }

    xl2515_write_reg_byte(CANINTF, 0);
    xl2515_write_reg_byte(CANINTE, 0x01);  // enable
    xl2515_write_reg_byte(RXB0SIDH, 0x00); // clean
    xl2515_write_reg_byte(RXB0SIDL, 0x60);
    return true;
}
#endif

bool xl2515_recv(uint32_t *can_id, uint8_t *data, uint8_t *len)
{
    uint8_t i;

    if ((xl2515_read_reg_byte(CANINTF) & 0x01) == 0) {
        return false;
    }
    *can_id = ((uint32_t)xl2515_read_reg_byte(RXB0SIDH) << 3) |
              (xl2515_read_reg_byte(RXB0SIDL) >> 5);
    *len = xl2515_read_reg_byte(RXB0DLC) & 0x0F;
    if (*len > 8) {
        *len = 8;
    }
    for (i = 0; i < *len; i++) {
        data[i] = xl2515_read_reg_byte(RXB0D0 + i);
    }
    xl2515_write_reg_byte(CANINTF, 0);
    return true;
}

