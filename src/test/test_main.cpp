// SPDX-License-Identifier: GPL-3.0+

#include <Arduino.h>
#include <unity.h>
#include <osd.hpp>

void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void test_format_osd_msg(void) {
    OSD osd;
    char buf[128];

    TEST_ASSERT_TRUE(osd.eval_format("%%", 0, 0, 0, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING(buf, "%");
    
    TEST_ASSERT_TRUE(osd.eval_format("[%L] %.1ts (%.2ds)", 2, 31311, -412, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("[2] 31.3 (-0.41)", buf);
    
    TEST_ASSERT_TRUE(osd.eval_format("[%L] %.1ts (%.2ds)", 2, 31351, -412, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("[2] 31.4 (-0.41)", buf);
    
    TEST_ASSERT_TRUE(osd.eval_format("[%L] %.1ts (%.3ds)", 2, 31311, 412, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("[2] 31.3 (+0.412)", buf);

    TEST_ASSERT_TRUE(osd.eval_format("[%L] %.1tmmin (%.3dmsms)", 2, 2*1000*60 + 1000*43 + 212, 412, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("[2] 2:43.2min (+412ms)", buf);

    TEST_ASSERT_TRUE(osd.eval_format("%.2tm", 2, 2 * 60 * 1000 + 42 * 1000 + 999, 412, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("2:43.00", buf);
    TEST_ASSERT_TRUE(osd.eval_format("%.2tm", 2, 2 * 60 * 1000 + 2 * 1000 + 999, 412, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("2:03.00", buf);
    TEST_ASSERT_TRUE(osd.eval_format("%.2dm", 2, 0, (2 * 60 * 1000 + 42 * 1000 + 999), buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("+2:43.00", buf);
    TEST_ASSERT_TRUE(osd.eval_format("%.2dm", 2, 0, -1 * (2 * 60 * 1000 + 42 * 1000 + 999), buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("-2:43.00", buf);
    TEST_ASSERT_TRUE(osd.eval_format("%.2dm", 2, 0, -1 * (2 * 60 * 1000 + 2 * 1000 + 999), buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("-2:03.00", buf);
}

void setup()
{
    delay(2000); // service delay
    UNITY_BEGIN();

    RUN_TEST(test_format_osd_msg);

    UNITY_END(); // stop unit testing
}

void loop()
{
}
