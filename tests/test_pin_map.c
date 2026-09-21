/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 */
#include "board_pins.h"
#include "unity.h"

void rr_run_lcc_tests(void);

void setUp(void) {}
void tearDown(void) {}

static void test_assigned_count(void)
{
    TEST_ASSERT_EQUAL_INT(17, rr_pin_assigned_count());
    TEST_ASSERT_EQUAL_INT(RR_LCD_DC_GPIO, rr_pin_at(0));
    TEST_ASSERT_EQUAL_INT(RR_CAN_INT2_GPIO, rr_pin_at(16));
    TEST_ASSERT_EQUAL_INT(-1, rr_pin_at(-1));
    TEST_ASSERT_EQUAL_INT(-1, rr_pin_at(17));
}

static void test_no_pin_conflicts(void)
{
    TEST_ASSERT_EQUAL_INT(0, rr_pin_conflict_count());
}

static void test_lcd_and_can_use_different_spi_pins(void)
{
    TEST_ASSERT_EQUAL_INT(10, RR_LCD_SCK_GPIO);
    TEST_ASSERT_EQUAL_INT(6, RR_CAN_SCK_GPIO);
    TEST_ASSERT_EQUAL_INT(11, RR_LCD_MOSI_GPIO);
    TEST_ASSERT_EQUAL_INT(7, RR_CAN_MOSI_GPIO);
    TEST_ASSERT_EQUAL_INT(4, RR_CAN_MISO_GPIO);
    TEST_ASSERT_EQUAL_INT(21, RR_CAN_INT_GPIO);
}

static void test_does_not_use_wireless_gpios(void)
{
    const int quiet[] = {RR_LCD_DC_GPIO, RR_CAN_CS_GPIO};
    const int on[] = {RR_WIFI_ON_GPIO};
    const int data[] = {RR_WIFI_DATA_GPIO};
    const int cs[] = {RR_WIFI_CS_GPIO};
    const int clk[] = {RR_WIFI_CLK_GPIO};

    TEST_ASSERT_EQUAL_INT(0, rr_pin_uses_wireless_gpio());
    TEST_ASSERT_EQUAL_INT(0, rr_pins_use_wireless(quiet, 2));
    TEST_ASSERT_EQUAL_INT(1, rr_pins_use_wireless(on, 1));
    TEST_ASSERT_EQUAL_INT(1, rr_pins_use_wireless(data, 1));
    TEST_ASSERT_EQUAL_INT(1, rr_pins_use_wireless(cs, 1));
    TEST_ASSERT_EQUAL_INT(1, rr_pins_use_wireless(clk, 1));
}

static void test_conflict_counter_sees_duplicates(void)
{
    const int same[] = {RR_LCD_CS_GPIO, RR_LCD_CS_GPIO};
    const int pair[] = {1, 2, 1};
    const int none[] = {1, 2, 3};

    TEST_ASSERT_EQUAL_INT(1, rr_count_conflicts(same, 2));
    TEST_ASSERT_EQUAL_INT(1, rr_count_conflicts(pair, 3));
    TEST_ASSERT_EQUAL_INT(0, rr_count_conflicts(none, 3));
    TEST_ASSERT_EQUAL_INT(0, rr_count_conflicts(none, 0));
}

static void test_key0_default_and_node_id(void)
{
    TEST_ASSERT_EQUAL_INT(15, RR_KEY0_GPIO);
    TEST_ASSERT_EQUAL_UINT64(0x05010101A505ULL, (unsigned long long)RR_NODE_ID_U64);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_assigned_count);
    RUN_TEST(test_no_pin_conflicts);
    RUN_TEST(test_lcd_and_can_use_different_spi_pins);
    RUN_TEST(test_does_not_use_wireless_gpios);
    RUN_TEST(test_conflict_counter_sees_duplicates);
    RUN_TEST(test_key0_default_and_node_id);
    rr_run_lcc_tests();
    return UNITY_END();
}
