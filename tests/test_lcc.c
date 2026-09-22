/*
 * Copyright (c) 2026 Charles L. Sherman
 * SPDX-License-Identifier: MIT
 */
#include "gridconnect.h"
#include "lcc_login.h"
#include "lcc_node_services.h"
#include "unity.h"

#include <string.h>

static void test_alias_and_port(void)
{
    TEST_ASSERT_EQUAL_INT(RR_GC_PORT, 12021);
    TEST_ASSERT_EQUAL_UINT16(RR_ALIAS_A505, rr_alias_from_node(0x05010101A505ULL));
    TEST_ASSERT_EQUAL_UINT16(1, rr_alias_from_node(0x050101010000ULL));
}

static void test_login_matches_openlcb_can_packing(void)
{
    char text[9 * RR_GC_FRAME_MAX];
    int n = rr_login_gridconnect(RR_ALIAS_A505, 0x05010101A505ULL, text, (int)sizeof text);

    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(text, ":X17050505N;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X16101505N;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X1501A505N;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X14505505N;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X10700505N;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X10701505N05010101A505;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X19100505N05010101A505;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X19547505N05010101A5050001;"));
    TEST_ASSERT_NOT_NULL(strstr(text, ":X194C7505N05010101A5050000;"));
    TEST_ASSERT_EQUAL_INT(-1, rr_login_gridconnect(0, 1, text, (int)sizeof text));
}

static void test_gridconnect_round_trip_and_verify_reply(void)
{
    char frame[RR_GC_FRAME_MAX];
    char reply[RR_GC_FRAME_MAX];
    rr_gc_parser parser;
    uint32_t identifier = 0;
    uint8_t data[8];
    int data_len = 0;
    const char *cursor;
    int i;

    TEST_ASSERT_EQUAL_INT(24, rr_gc_format(frame, (int)sizeof frame, 0x19100505u,
                                           (const uint8_t *)"\x05\x01\x01\x01\xA5\x05", 6));
    TEST_ASSERT_EQUAL_STRING(":X19100505N05010101A505;", frame);
    TEST_ASSERT_EQUAL_INT(-1, rr_gc_format(frame, 4, 1, 0, 0));

    rr_gc_parser_init(&parser);
    cursor = "junk:X19490000N;";
    for (i = 0; cursor[i] != '\0'; i++) {
        if (rr_gc_feed(&parser, cursor[i], &identifier, data, &data_len)) {
            break;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0x19490000u, identifier);
    TEST_ASSERT_EQUAL_INT(0, data_len);

    i = rr_reply_gridconnect(0x19490000u, 0, 0, RR_ALIAS_A505, 0x05010101A505ULL,
                             reply, (int)sizeof reply);
    TEST_ASSERT_GREATER_THAN(0, i);
    TEST_ASSERT_EQUAL_STRING(":X19170505N05010101A505;", reply);
    TEST_ASSERT_EQUAL_INT(0, rr_reply_gridconnect(0x10700505u, 0, 0, RR_ALIAS_A505,
                                                  0x05010101A505ULL, reply, (int)sizeof reply));

    data[0] = 0x05;
    data[1] = 0x05;
    i = rr_reply_gridconnect(0x19488000u | RR_ALIAS_A505, data, 2, RR_ALIAS_A505,
                             0x05010101A505ULL, reply, (int)sizeof reply);
    TEST_ASSERT_GREATER_THAN(0, i);
    data[1] = 0x06;
    TEST_ASSERT_EQUAL_INT(0, rr_reply_gridconnect(0x19488505u, data, 2, RR_ALIAS_A505,
                                                  1, reply, (int)sizeof reply));
}

static void test_snip_and_cdi_read(void)
{
    char out[512];
    uint8_t dest[2] = {0x05, 0x05};
    uint8_t read_cmd[7] = {0x20, 0x43, 0, 0, 0, 0, 8};
    uint32_t snip_id = 0x19000000u | (0x0DE8u << 12) | 0x123u;
    uint32_t cdi_id = 0x1A000000u | ((uint32_t)RR_ALIAS_A505 << 12) | 0x123u;
    int n = rr_service_frame(snip_id, dest, 2, RR_ALIAS_A505, 0x05010101A505ULL,
                             out, (int)sizeof out);

    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(out, "4F776C5468"));
    n = rr_service_frame(cdi_id, read_cmd, 7, RR_ALIAS_A505, 0x05010101A505ULL,
                         out, (int)sizeof out);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(out, "3C3F"));
    TEST_ASSERT_NOT_NULL(strstr(rr_cdi_xml(), "Pico2W-A505"));
}

static void test_consumer_event(void)
{
    char out[32];
    uint8_t event[8] = {0x05, 0x01, 0x01, 0x01, 0xA5, 0x05, 0x00, 0x00};
    uint32_t pcer = 0x19000000u | (0x05B4u << 12) | 0x123u;
    int before = rr_consumer_hits();

    TEST_ASSERT_EQUAL_INT(0, rr_service_frame(pcer, event, 8, RR_ALIAS_A505,
                                              0x05010101A505ULL, out, (int)sizeof out));
    TEST_ASSERT_EQUAL_INT(before + 1, rr_consumer_hits());
}

static int feed_all(const char *text, uint32_t *identifier, uint8_t *data, int *data_len)
{
    rr_gc_parser parser;
    int i;
    int hits = 0;

    rr_gc_parser_init(&parser);
    for (i = 0; text[i] != '\0'; i++) {
        if (rr_gc_feed(&parser, text[i], identifier, data, data_len)) {
            hits++;
        }
    }
    return hits;
}

static void test_gridconnect_rejects_bad_frames(void)
{
    char frame[8];
    uint32_t identifier = 0;
    uint8_t data[8];
    int data_len = 0;

    TEST_ASSERT_EQUAL_INT(-1, rr_gc_format(frame, (int)sizeof frame, 1, 0, 9));
    TEST_ASSERT_EQUAL_INT(-1, rr_gc_format(frame, 20, 1, 0, 1));
    TEST_ASSERT_EQUAL_INT(0, feed_all(":X1949000gN;", &identifier, data, &data_len));
    TEST_ASSERT_EQUAL_INT(1, feed_all(":X19490000Naa;", &identifier, data, &data_len));
    TEST_ASSERT_EQUAL_INT(0, feed_all(":X19490000N0123456789ABCDEF0;", &identifier, data, &data_len));
}

static void test_pip_events_and_datagram_reject(void)
{
    char out[256];
    uint8_t dest[2] = {0x05, 0x05};
    uint8_t odd[7] = {0x20, 0x00, 0, 0, 0, 0, 80};
    uint32_t pip = 0x19000000u | (0x0828u << 12) | 0x123u;
    uint32_t events = 0x19000000u | (0x0970u << 12) | 0x123u;
    uint32_t bad = 0x1A000000u | ((uint32_t)RR_ALIAS_A505 << 12) | 0x123u;
    int n = rr_service_frame(pip, dest, 2, RR_ALIAS_A505, 0x05010101A505ULL,
                             out, (int)sizeof out);

    TEST_ASSERT_NOT_NULL(strstr(out, "5418"));
    n = rr_service_frame(events, 0, 0, RR_ALIAS_A505, 0x05010101A505ULL,
                         out, (int)sizeof out);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_NOT_NULL(strstr(out, "19547505"));
    n = rr_service_frame(bad, odd, 7, RR_ALIAS_A505, 0x05010101A505ULL,
                         out, (int)sizeof out);
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_TRUE(rr_service_frame(pip, dest, 2, RR_ALIAS_A505,
                                     0x05010101A505ULL, out, 4) < 0);
}

void rr_run_lcc_tests(void)
{
    RUN_TEST(test_alias_and_port);
    RUN_TEST(test_login_matches_openlcb_can_packing);
    RUN_TEST(test_gridconnect_round_trip_and_verify_reply);
    RUN_TEST(test_gridconnect_rejects_bad_frames);
    RUN_TEST(test_snip_and_cdi_read);
    RUN_TEST(test_consumer_event);
    RUN_TEST(test_pip_events_and_datagram_reject);
}
