#include <message_queue.h>
#include <queue_codec.h>
#include <string.h>
#include <unity.h>

using namespace mq;

static MessageQueue* source;
static MessageQueue* target;
static uint8_t buffer[kMaxEncodedSize];

void setUp()
{
    source = new MessageQueue();
    target = new MessageQueue();
}

void tearDown()
{
    delete source;
    delete target;
}

static void addFull(MessageQueue& queue, const char* id, const char* title, const char* value, FontSize fontSize,
                    Color color, Kind kind, uint32_t durationS, int64_t receivedAt = 0)
{
    TEST_ASSERT_EQUAL(AddResult::Added,
                      queue.add(NewMessage{id, title, value, fontSize, color, kind, durationS, receivedAt}));
}

// Rewrites the checksum after a test edits the encoded bytes.
static void resign(size_t size)
{
    const uint32_t hash = fnv1a(buffer, size - 4);
    for (int i = 0; i < 4; i++) {
        buffer[size - 4 + i] = static_cast<uint8_t>(hash >> (8 * i));
    }
}

static void test_round_trip_keeps_order_and_fields()
{
    addFull(*source, "a", "Title A", "{green}ok{/}", FontSize::Large, Color::Blue, Kind::Timed, 30, 1790000000);
    addFull(*source, "", "", "no id", FontSize::Small, Color::Red, Kind::Confirm, 0);
    addFull(*source, "c", "Title C", "line1\nline2", FontSize::Small, Color::White, Kind::Confirm, 0, 4000000000);

    const size_t size = encodeQueue(*source, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, size);

    size_t restored = 0;
    TEST_ASSERT_TRUE(decodeQueue(buffer, size, *target, restored));
    TEST_ASSERT_EQUAL(3, restored);
    TEST_ASSERT_EQUAL(3, target->size());
    for (size_t i = 0; i < 3; i++) {
        const Message& a = source->at(i);
        const Message& b = target->at(i);
        TEST_ASSERT_EQUAL_STRING(a.id, b.id);
        TEST_ASSERT_EQUAL_STRING(a.title, b.title);
        TEST_ASSERT_EQUAL_STRING(a.value, b.value);
        TEST_ASSERT_EQUAL(a.fontSize, b.fontSize);
        TEST_ASSERT_EQUAL(a.color, b.color);
        TEST_ASSERT_EQUAL(a.kind, b.kind);
        TEST_ASSERT_EQUAL_UINT32(a.durationS, b.durationS);
        TEST_ASSERT_EQUAL_INT64(a.receivedAt, b.receivedAt);
    }
    TEST_ASSERT_EQUAL_INT64(4000000000, target->at(0).receivedAt);
    TEST_ASSERT_EQUAL_INT64(0, target->at(1).receivedAt);
    TEST_ASSERT_EQUAL(0, target->cursor());
}

static void test_timed_message_restarts_full_duration()
{
    addFull(*source, "t", "T", "timed", FontSize::Small, Color::White, Kind::Timed, 10);
    source->tick(0);
    source->tick(7000);
    TEST_ASSERT_EQUAL_UINT32(3000, source->current()->remainingMs);

    const size_t size = encodeQueue(*source, buffer, sizeof(buffer));
    size_t restored = 0;
    TEST_ASSERT_TRUE(decodeQueue(buffer, size, *target, restored));
    TEST_ASSERT_EQUAL_UINT32(10000, target->current()->remainingMs);
}

static void test_empty_queue_round_trip()
{
    const size_t size = encodeQueue(*source, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(8, size);
    addFull(*target, "x", "X", "existing", FontSize::Small, Color::White, Kind::Confirm, 0);
    size_t restored = 1;
    TEST_ASSERT_TRUE(decodeQueue(buffer, size, *target, restored));
    TEST_ASSERT_EQUAL(0, restored);
    TEST_ASSERT_TRUE(target->empty());
}

static void test_full_queue_at_maximum_lengths_fits()
{
    char id[kIdMaxLen + 1];
    char title[kTitleMaxLen + 1];
    char value[kValueMaxLen + 1];
    memset(title, 't', kTitleMaxLen);
    title[kTitleMaxLen] = '\0';
    memset(value, 'v', kValueMaxLen);
    value[kValueMaxLen] = '\0';
    for (size_t i = 0; i < kCapacity; i++) {
        memset(id, 'a' + static_cast<char>(i % 26), kIdMaxLen);
        id[kIdMaxLen] = '\0';
        id[0] = static_cast<char>('0' + i / 26);  // keep ids unique
        addFull(*source, id, title, value, FontSize::Small, Color::White, Kind::Timed, kMaxDurationS, 4294967295);
    }
    TEST_ASSERT_EQUAL(kMaxEncodedSize, encodeQueue(*source, buffer, sizeof(buffer)));
    size_t restored = 0;
    TEST_ASSERT_TRUE(decodeQueue(buffer, kMaxEncodedSize, *target, restored));
    TEST_ASSERT_EQUAL(kCapacity, restored);
}

static void test_encode_fails_when_capacity_too_small()
{
    addFull(*source, "a", "A", "value", FontSize::Small, Color::White, Kind::Confirm, 0);
    const size_t size = encodeQueue(*source, buffer, sizeof(buffer));
    TEST_ASSERT_EQUAL(0, encodeQueue(*source, buffer, size - 1));
    TEST_ASSERT_EQUAL(size, encodeQueue(*source, buffer, size));
}

static void test_decode_rejects_bad_data_and_leaves_queue_unchanged()
{
    addFull(*source, "a", "A", "value", FontSize::Small, Color::White, Kind::Confirm, 0);
    const size_t size = encodeQueue(*source, buffer, sizeof(buffer));
    addFull(*target, "keep", "K", "keep me", FontSize::Small, Color::White, Kind::Confirm, 0);
    size_t restored = 0;

    buffer[10] ^= 0x01;  // corrupt a byte: checksum mismatch
    TEST_ASSERT_FALSE(decodeQueue(buffer, size, *target, restored));
    buffer[10] ^= 0x01;

    TEST_ASSERT_FALSE(decodeQueue(buffer, size - 1, *target, restored));  // truncated
    TEST_ASSERT_FALSE(decodeQueue(buffer, 3, *target, restored));         // shorter than header + checksum

    buffer[2] = kCodecVersion + 1;  // unknown version
    resign(size);
    TEST_ASSERT_FALSE(decodeQueue(buffer, size, *target, restored));
    buffer[2] = 0;
    resign(size);
    TEST_ASSERT_FALSE(decodeQueue(buffer, size, *target, restored));
    buffer[2] = kCodecVersion;

    buffer[3] = 2;  // claims more messages than present
    resign(size);
    TEST_ASSERT_FALSE(decodeQueue(buffer, size, *target, restored));

    TEST_ASSERT_EQUAL(1, target->size());
    TEST_ASSERT_EQUAL_STRING("keep me", target->current()->value);
}

static void test_decode_skips_messages_the_queue_rejects()
{
    addFull(*source, "a", "A", "good", FontSize::Small, Color::White, Kind::Confirm, 0);
    addFull(*source, "b", "B", "bad color", FontSize::Small, Color::White, Kind::Confirm, 0);
    const size_t size = encodeQueue(*source, buffer, sizeof(buffer));
    buffer[4 + 2] = 9;  // first entry (newest, "b"): color byte out of range
    resign(size);

    size_t restored = 0;
    TEST_ASSERT_TRUE(decodeQueue(buffer, size, *target, restored));
    TEST_ASSERT_EQUAL(1, restored);
    TEST_ASSERT_EQUAL_STRING("good", target->current()->value);
}

// Files saved before 0.0.30 use version 1, without the arrival time.
static void test_decode_version_1_without_arrival_time()
{
    const uint8_t v1[] = {'S', 'Q', 1, 1,                     // header, one message
                          1, 0, 2, 0, 0, 0, 0,                // confirm, small, green, duration 0
                          1, 'a', 1, 'T', 2, 0, 'h', 'i',    // id "a", title "T", value "hi"
                          0, 0, 0, 0};                        // checksum, set below
    memcpy(buffer, v1, sizeof(v1));
    resign(sizeof(v1));
    size_t restored = 0;
    TEST_ASSERT_TRUE(decodeQueue(buffer, sizeof(v1), *target, restored));
    TEST_ASSERT_EQUAL(1, restored);
    const Message& message = target->at(0);
    TEST_ASSERT_EQUAL_STRING("a", message.id);
    TEST_ASSERT_EQUAL_STRING("T", message.title);
    TEST_ASSERT_EQUAL_STRING("hi", message.value);
    TEST_ASSERT_EQUAL(Color::Green, message.color);
    TEST_ASSERT_EQUAL(Kind::Confirm, message.kind);
    TEST_ASSERT_EQUAL_INT64(0, message.receivedAt);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_round_trip_keeps_order_and_fields);
    RUN_TEST(test_timed_message_restarts_full_duration);
    RUN_TEST(test_empty_queue_round_trip);
    RUN_TEST(test_full_queue_at_maximum_lengths_fits);
    RUN_TEST(test_encode_fails_when_capacity_too_small);
    RUN_TEST(test_decode_rejects_bad_data_and_leaves_queue_unchanged);
    RUN_TEST(test_decode_skips_messages_the_queue_rejects);
    RUN_TEST(test_decode_version_1_without_arrival_time);
    return UNITY_END();
}
