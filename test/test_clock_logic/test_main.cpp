#include <clock_logic.h>
#include <string.h>
#include <unity.h>

using namespace clk;

void setUp() {}
void tearDown() {}

static bool parse(const char* json, int32_t& offset, char* zone)
{
    return parseTimezone(json, strlen(json), offset, zone, 40);
}

static void test_parse_success()
{
    int32_t offset = 0;
    char zone[40];
    TEST_ASSERT_TRUE(parse(R"({"status":"success","timezone":"Europe/Berlin","offset":7200})", offset, zone));
    TEST_ASSERT_EQUAL_INT32(7200, offset);
    TEST_ASSERT_EQUAL_STRING("Europe/Berlin", zone);
    TEST_ASSERT_TRUE(parse(R"({"status":"success","timezone":"America/St_Johns","offset":-9000})", offset, zone));
    TEST_ASSERT_EQUAL_INT32(-9000, offset);
}

static void test_parse_failures_leave_offset_unchanged()
{
    int32_t offset = 123;
    char zone[40];
    TEST_ASSERT_FALSE(parse(R"({"status":"fail","message":"private range"})", offset, zone));
    TEST_ASSERT_FALSE(parse(R"({"status":"success","timezone":"X"})", offset, zone));
    TEST_ASSERT_FALSE(parse(R"({"status":"success","offset":"7200"})", offset, zone));
    TEST_ASSERT_FALSE(parse(R"({"status":"success","offset":90000})", offset, zone));
    TEST_ASSERT_FALSE(parse("not json", offset, zone));
    TEST_ASSERT_EQUAL_INT32(123, offset);
}

static void test_parse_missing_zone_name_is_empty()
{
    int32_t offset = 0;
    char zone[40] = "old";
    TEST_ASSERT_TRUE(parse(R"({"status":"success","offset":0})", offset, zone));
    TEST_ASSERT_EQUAL_STRING("", zone);
}

static void test_format_clock()
{
    char text[8];
    formatClock(0, 0, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("00:00", text);
    formatClock(1790000000, 0, text, sizeof(text));  // 2026-09-21 14:13:20 UTC
    TEST_ASSERT_EQUAL_STRING("14:13", text);
    formatClock(1790000000, 7200, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("16:13", text);
    formatClock(1790000000, 36000, text, sizeof(text));  // past midnight
    TEST_ASSERT_EQUAL_STRING("00:13", text);
    formatClock(1790000000, -9000 * 2, text, sizeof(text));  // UTC-5
    TEST_ASSERT_EQUAL_STRING("09:13", text);
    formatClock(3599, -7200, text, sizeof(text));  // before the epoch in local time
    TEST_ASSERT_EQUAL_STRING("22:59", text);
}

static void test_http_body()
{
    const char* ok = "HTTP/1.0 200 OK\r\nContent-Type: application/json\r\n\r\n{\"a\":1}";
    const char* body = nullptr;
    size_t length = 0;
    TEST_ASSERT_TRUE(httpBody(ok, strlen(ok), body, length));
    TEST_ASSERT_EQUAL(7, length);
    TEST_ASSERT_EQUAL_MEMORY("{\"a\":1}", body, 7);

    const char* notFound = "HTTP/1.1 404 Not Found\r\n\r\nnope";
    TEST_ASSERT_FALSE(httpBody(notFound, strlen(notFound), body, length));
    const char* noEnd = "HTTP/1.1 200 OK\r\nContent-Length: 5";
    TEST_ASSERT_FALSE(httpBody(noEnd, strlen(noEnd), body, length));
    TEST_ASSERT_FALSE(httpBody("", 0, body, length));
    TEST_ASSERT_FALSE(httpBody("garbage 200 xx\r\n\r\n", 18, body, length));
}

static void test_format_arrival()
{
    char text[24];
    // 2026-09-24 22:30:00 UTC; with +2 h it is 00:30 on the 25th locally.
    const int64_t received = 1790289000;
    formatArrival(received, received + 3600, 0, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("22:30", text);  // same day
    formatArrival(received, received + 2 * 3600, 0, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("22:30 24-09-2026", text);  // past midnight UTC
    formatArrival(received, received + 2 * 3600, 7200, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("00:30", text);  // both on the 25th locally
    formatArrival(received, received + 30 * 86400, 7200, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("00:30 25-09-2026", text);
    formatArrival(received, 0, 0, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("22:30 24-09-2026", text);  // current time unknown
    formatArrival(received, received, -9 * 3600, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("13:30", text);  // negative offset
}

static void test_format_arrival_dates()
{
    char text[24];
    formatArrival(0, 1, 0, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("00:00", text);
    formatArrival(951782400, 1, 0, text, sizeof(text));  // leap day 2000
    TEST_ASSERT_EQUAL_STRING("00:00 29-02-2000", text);
    formatArrival(1735689599, 1, 0, text, sizeof(text));  // last second of 2024
    TEST_ASSERT_EQUAL_STRING("23:59 31-12-2024", text);
    formatArrival(1735689600, 1, 0, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("00:00 01-01-2025", text);
    formatArrival(4294967295LL, 1, 0, text, sizeof(text));  // largest saved value
    TEST_ASSERT_EQUAL_STRING("06:28 07-02-2106", text);
}

static void test_format_date()
{
    char text[12];
    formatDate(1790289000, 0, text, sizeof(text));  // 2026-09-24 22:30 UTC
    TEST_ASSERT_EQUAL_STRING("24-09-2026", text);
    formatDate(1790289000, 7200, text, sizeof(text));  // 00:30 on the 25th locally
    TEST_ASSERT_EQUAL_STRING("25-09-2026", text);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_success);
    RUN_TEST(test_parse_failures_leave_offset_unchanged);
    RUN_TEST(test_parse_missing_zone_name_is_empty);
    RUN_TEST(test_format_clock);
    RUN_TEST(test_http_body);
    RUN_TEST(test_format_arrival);
    RUN_TEST(test_format_arrival_dates);
    RUN_TEST(test_format_date);
    return UNITY_END();
}
