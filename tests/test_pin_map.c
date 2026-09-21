/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 */
#include "board_pins.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void test_assigned_count(void)
{
    TEST_ASSERT_EQUAL_INT(17, rr_pin_assigned_count());
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
    TEST_ASSERT_EQUAL_INT(0, rr_pin_uses_wireless_gpio());
    TEST_ASSERT_EQUAL_INT(23, RR_WIFI_ON_GPIO);
    TEST_ASSERT_EQUAL_INT(29, RR_WIFI_CLK_GPIO);
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
    RUN_TEST(test_key0_default_and_node_id);
    return UNITY_END();
}
