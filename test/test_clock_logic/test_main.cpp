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

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_parse_success);
    RUN_TEST(test_parse_failures_leave_offset_unchanged);
    RUN_TEST(test_parse_missing_zone_name_is_empty);
    RUN_TEST(test_format_clock);
    RUN_TEST(test_http_body);
    return UNITY_END();
}
