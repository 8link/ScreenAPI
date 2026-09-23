#include <ArduinoJson.h>
#include <mcp_handler.h>
#include <message_queue.h>
#include <string.h>
#include <text_clean.h>
#include <unity.h>

#include <string>

using namespace mcp;

static mq::MessageQueue* queue;
static Handler* handler;
static JsonDocument reply;

void setUp()
{
    queue = new mq::MessageQueue();
    handler = new Handler(*queue, "9.9.9");
}

void tearDown()
{
    delete handler;
    delete queue;
}

static Response post(const std::string& body)
{
    const Response response = handler->handlePost(body.c_str(), body.size());
    reply.clear();
    if (!response.body.empty()) {
        TEST_ASSERT_EQUAL(DeserializationError::Ok, deserializeJson(reply, response.body).code());
    }
    return response;
}

static Response callTool(const char* name, const std::string& arguments)
{
    return post(std::string(R"({"jsonrpc":"2.0","id":7,"method":"tools/call","params":{"name":")") + name +
                R"(","arguments":)" + arguments + "}}");
}

static const char* toolText()
{
    return reply["result"]["content"][0]["text"];
}

static bool toolIsError()
{
    return reply["result"]["isError"];
}

static void test_initialize_echoes_supported_version()
{
    const Response response = post(
        R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18","capabilities":{},"clientInfo":{"name":"test","version":"1"}}})");
    TEST_ASSERT_EQUAL(200, response.httpStatus);
    TEST_ASSERT_EQUAL(1, reply["id"].as<int>());
    TEST_ASSERT_EQUAL_STRING("2025-06-18", reply["result"]["protocolVersion"]);
    TEST_ASSERT_EQUAL_STRING("ScreenAPI", reply["result"]["serverInfo"]["name"]);
    TEST_ASSERT_EQUAL_STRING("9.9.9", reply["result"]["serverInfo"]["version"]);
    TEST_ASSERT_FALSE(reply["result"]["capabilities"]["tools"]["listChanged"].as<bool>());
    TEST_ASSERT_NOT_NULL(reply["result"]["instructions"].as<const char*>());
}

static void test_initialize_unknown_version_gets_latest()
{
    post(R"({"jsonrpc":"2.0","id":"a","method":"initialize","params":{"protocolVersion":"1999-01-01"}})");
    TEST_ASSERT_EQUAL_STRING("a", reply["id"]);
    TEST_ASSERT_EQUAL_STRING("2025-11-25", reply["result"]["protocolVersion"]);
}

static void test_notifications_and_client_responses_get_202()
{
    Response response = post(R"({"jsonrpc":"2.0","method":"notifications/initialized"})");
    TEST_ASSERT_EQUAL(202, response.httpStatus);
    TEST_ASSERT_TRUE(response.body.empty());
    response = post(R"({"jsonrpc":"2.0","id":3,"result":{}})");
    TEST_ASSERT_EQUAL(202, response.httpStatus);
}

static void test_ping()
{
    post(R"({"jsonrpc":"2.0","id":2,"method":"ping"})");
    TEST_ASSERT_TRUE(reply["result"].is<JsonObject>());
}

static void test_tools_list()
{
    post(R"({"jsonrpc":"2.0","id":4,"method":"tools/list"})");
    JsonArray tools = reply["result"]["tools"];
    TEST_ASSERT_EQUAL(2, tools.size());
    TEST_ASSERT_EQUAL_STRING("show_message", tools[0]["name"]);
    TEST_ASSERT_EQUAL_STRING("object", tools[0]["inputSchema"]["type"]);
    TEST_ASSERT_EQUAL_STRING("value", tools[0]["inputSchema"]["required"][0]);
    TEST_ASSERT_EQUAL(512, tools[0]["inputSchema"]["properties"]["value"]["maxLength"].as<int>());
    TEST_ASSERT_EQUAL(4, tools[0]["inputSchema"]["properties"]["color"]["enum"].size());
    TEST_ASSERT_EQUAL_STRING("queue_status", tools[1]["name"]);
}

static void test_protocol_errors()
{
    Response response = post("{not json");
    TEST_ASSERT_EQUAL(400, response.httpStatus);
    TEST_ASSERT_EQUAL(-32700, reply["error"]["code"].as<int>());

    response = post(R"([{"jsonrpc":"2.0","id":1,"method":"ping"}])");
    TEST_ASSERT_EQUAL(400, response.httpStatus);
    TEST_ASSERT_EQUAL(-32600, reply["error"]["code"].as<int>());

    response = post(R"({"jsonrpc":"1.0","id":1,"method":"ping"})");
    TEST_ASSERT_EQUAL(-32600, reply["error"]["code"].as<int>());

    response = post(R"({"jsonrpc":"2.0","id":5,"method":"resources/list"})");
    TEST_ASSERT_EQUAL(200, response.httpStatus);
    TEST_ASSERT_EQUAL(-32601, reply["error"]["code"].as<int>());
    TEST_ASSERT_EQUAL(5, reply["id"].as<int>());

    callTool("no_such_tool", "{}");
    TEST_ASSERT_EQUAL(-32602, reply["error"]["code"].as<int>());

    const std::string big(kMaxRequestSize + 1, ' ');
    response = post(big);
    TEST_ASSERT_EQUAL(413, response.httpStatus);
}

static void test_show_message_defaults_to_confirm_white_small()
{
    const Response response = callTool("show_message", R"({"title":"Claude Code","value":"Working"})");
    TEST_ASSERT_FALSE(toolIsError());
    TEST_ASSERT_TRUE(response.queueChanged);
    TEST_ASSERT_NOT_NULL(strstr(toolText(), "Queue: 1 of 30"));
    const mq::Message* message = queue->current();
    TEST_ASSERT_EQUAL_STRING("Claude Code", message->title);
    TEST_ASSERT_EQUAL_STRING("Working", message->value);
    TEST_ASSERT_EQUAL(mq::Kind::Confirm, message->kind);
    TEST_ASSERT_EQUAL(mq::Color::White, message->color);
    TEST_ASSERT_EQUAL(mq::FontSize::Small, message->fontSize);
}

static void test_show_message_all_fields()
{
    callTool("show_message",
             R"({"id":"build","title":"Build","value":"{green}ok{/}","color":"blue","font_size":"large","kind":"timed","duration_s":45})");
    TEST_ASSERT_FALSE(toolIsError());
    const mq::Message* message = queue->current();
    TEST_ASSERT_EQUAL_STRING("build", message->id);
    TEST_ASSERT_EQUAL_STRING("{green}ok{/}", message->value);
    TEST_ASSERT_EQUAL(mq::Color::Blue, message->color);
    TEST_ASSERT_EQUAL(mq::FontSize::Large, message->fontSize);
    TEST_ASSERT_EQUAL(mq::Kind::Timed, message->kind);
    TEST_ASSERT_EQUAL_UINT32(45, message->durationS);
}

static void test_duration_alone_means_timed()
{
    callTool("show_message", R"({"value":"v","duration_s":10})");
    TEST_ASSERT_EQUAL(mq::Kind::Timed, queue->current()->kind);
    callTool("show_message", R"({"value":"v","kind":"confirm","duration_s":10})");
    TEST_ASSERT_EQUAL(mq::Kind::Confirm, queue->current()->kind);
}

static void test_show_message_replaces_by_id()
{
    callTool("show_message", R"({"id":"status","value":"one"})");
    callTool("show_message", R"({"id":"status","value":"two"})");
    TEST_ASSERT_FALSE(toolIsError());
    TEST_ASSERT_NOT_NULL(strstr(toolText(), "Replaced"));
    TEST_ASSERT_EQUAL(1, queue->size());
    TEST_ASSERT_EQUAL_STRING("two", queue->current()->value);
}

static void test_show_message_validation_errors()
{
    const char* const cases[][2] = {
        {R"({})", "value is required"},
        {R"({"value":""})", "value is required"},
        {R"({"value":5})", "value must be a string"},
        {R"({"value":"v","color":"yellow"})", "color must be one of: white blue green red"},
        {R"({"value":"v","font_size":"huge"})", "font_size must be one of"},
        {R"({"value":"v","kind":"timed"})", "needs duration_s"},
        {R"({"value":"v","duration_s":0})", "duration_s must be a whole number from 1 to 86400"},
        {R"({"value":"v","duration_s":90000})", "duration_s must be"},
        {R"({"value":"v","duration_s":1.5})", "duration_s must be"},
        {R"({"value":"v","id":"12345678901234567"})", "id is 17 characters; the limit is 16"},
    };
    for (const auto& c : cases) {
        const Response response = callTool("show_message", c[0]);
        TEST_ASSERT_TRUE_MESSAGE(toolIsError(), c[0]);
        TEST_ASSERT_NOT_NULL_MESSAGE(strstr(toolText(), c[1]), toolText());
        TEST_ASSERT_FALSE(response.queueChanged);
    }
    TEST_ASSERT_TRUE(queue->empty());
}

static void test_show_message_length_limit_counts_after_cleanup()
{
    std::string value(mq::kValueMaxLen, 'x');
    callTool("show_message", R"({"value":")" + value + R"("})");
    TEST_ASSERT_FALSE(toolIsError());
    // An ellipsis becomes three characters, which pushes the value over the limit.
    value = std::string(mq::kValueMaxLen - 2, 'x') + "\\u2026";  // JSON escape for an ellipsis
    callTool("show_message", R"({"value":")" + value + R"("})");
    TEST_ASSERT_TRUE(toolIsError());
    TEST_ASSERT_NOT_NULL(strstr(toolText(), "value is 513 characters; the limit is 512"));
}

static void test_show_message_reports_replaced_characters()
{
    callTool("show_message", R"({"title":"Build \u2014 done","value":"caf\u00e9 \ud83d\ude80 ok"})");
    TEST_ASSERT_FALSE(toolIsError());
    TEST_ASSERT_EQUAL_STRING("Build - done", queue->current()->title);
    TEST_ASSERT_EQUAL_STRING("caf? ? ok", queue->current()->value);
    TEST_ASSERT_NOT_NULL(strstr(toolText(), "2 non-ASCII characters were shown as '?'"));
}

static void test_full_queue_drops_and_reports()
{
    for (size_t i = 0; i < mq::kCapacity; i++) {
        callTool("show_message", R"({"value":"m"})");
    }
    const Response response = callTool("show_message", R"({"value":"one too many"})");
    TEST_ASSERT_TRUE(toolIsError());
    TEST_ASSERT_TRUE(response.messageDropped);
    TEST_ASSERT_FALSE(response.queueChanged);
    TEST_ASSERT_NOT_NULL(strstr(toolText(), "Queue full (30 of 30)"));
}

static void test_queue_status()
{
    callTool("show_message", R"({"value":"a"})");
    callTool("queue_status", "{}");
    TEST_ASSERT_FALSE(toolIsError());
    TEST_ASSERT_EQUAL_STRING("Queue: 1 of 30 messages.", toolText());
}

static void test_clean_text()
{
    char out[32];
    size_t replaced = 0;
    // en dash, curly quotes, ellipsis as UTF-8 bytes
    const char* typographic = "a\xE2\x80\x93" "b \xE2\x80\x9C" "q\xE2\x80\x9D\xE2\x80\xA6";
    TEST_ASSERT_EQUAL(10, cleanText(typographic, out, sizeof(out), replaced));
    TEST_ASSERT_EQUAL_STRING("a-b \"q\"...", out);
    TEST_ASSERT_EQUAL(0, replaced);

    cleanText("x\r\ny\tz\x01", out, sizeof(out), replaced);
    TEST_ASSERT_EQUAL_STRING("x\ny z", out);

    cleanText("\xff\xc3", out, sizeof(out), replaced);  // invalid and truncated UTF-8
    TEST_ASSERT_EQUAL_STRING("??", out);
    TEST_ASSERT_EQUAL(2, replaced);

    TEST_ASSERT_EQUAL(6, cleanText("abcdef", out, 4, replaced));  // too long: truncated, full length returned
    TEST_ASSERT_EQUAL_STRING("abc", out);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_initialize_echoes_supported_version);
    RUN_TEST(test_initialize_unknown_version_gets_latest);
    RUN_TEST(test_notifications_and_client_responses_get_202);
    RUN_TEST(test_ping);
    RUN_TEST(test_tools_list);
    RUN_TEST(test_protocol_errors);
    RUN_TEST(test_show_message_defaults_to_confirm_white_small);
    RUN_TEST(test_show_message_all_fields);
    RUN_TEST(test_duration_alone_means_timed);
    RUN_TEST(test_show_message_replaces_by_id);
    RUN_TEST(test_show_message_validation_errors);
    RUN_TEST(test_show_message_length_limit_counts_after_cleanup);
    RUN_TEST(test_show_message_reports_replaced_characters);
    RUN_TEST(test_full_queue_drops_and_reports);
    RUN_TEST(test_queue_status);
    RUN_TEST(test_clean_text);
    return UNITY_END();
}
