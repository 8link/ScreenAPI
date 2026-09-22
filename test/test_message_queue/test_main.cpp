#include <message_queue.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>

using namespace mq;

static MessageQueue* queue;

static NewMessage confirmMessage(const char* value, const char* id = nullptr)
{
    return NewMessage{id, "Title", value, FontSize::Small, Color::White, Kind::Confirm, 0};
}

static NewMessage timedMessage(const char* value, uint32_t durationS, const char* id = nullptr)
{
    return NewMessage{id, "Title", value, FontSize::Small, Color::White, Kind::Timed, durationS};
}

static void fill(size_t count)
{
    char value[8];
    for (size_t i = 0; i < count; i++) {
        snprintf(value, sizeof(value), "m%u", static_cast<unsigned>(i));
        TEST_ASSERT_EQUAL(AddResult::Added, queue->add(confirmMessage(value), 0));
    }
}

void setUp()
{
    queue = new MessageQueue();
}

void tearDown()
{
    delete queue;
}

static void test_starts_empty()
{
    TEST_ASSERT_TRUE(queue->empty());
    TEST_ASSERT_NULL(queue->current());
    TEST_ASSERT_FALSE(queue->deleteCurrent());
}

static void test_add_copies_fields()
{
    NewMessage input{"build", "Claude Code", "Running tests", FontSize::Large, Color::Green, Kind::Timed, 5};
    TEST_ASSERT_EQUAL(AddResult::Added, queue->add(input, 1000));

    const Message* message = queue->current();
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL_STRING("build", message->id);
    TEST_ASSERT_EQUAL_STRING("Claude Code", message->title);
    TEST_ASSERT_EQUAL_STRING("Running tests", message->value);
    TEST_ASSERT_EQUAL(FontSize::Large, message->fontSize);
    TEST_ASSERT_EQUAL(Color::Green, message->color);
    TEST_ASSERT_EQUAL(Kind::Timed, message->kind);
    TEST_ASSERT_EQUAL_UINT32(5, message->durationS);
    TEST_ASSERT_EQUAL_UINT64(6000, message->expiresAtMs);
}

static void test_null_id_and_title_are_empty()
{
    NewMessage input{nullptr, nullptr, "v", FontSize::Small, Color::White, Kind::Confirm, 0};
    TEST_ASSERT_EQUAL(AddResult::Added, queue->add(input, 0));
    TEST_ASSERT_EQUAL_STRING("", queue->current()->id);
    TEST_ASSERT_EQUAL_STRING("", queue->current()->title);
}

static void test_rejects_invalid_input()
{
    char longValue[kValueMaxLen + 2];
    memset(longValue, 'x', kValueMaxLen + 1);
    longValue[kValueMaxLen + 1] = '\0';
    char longTitle[kTitleMaxLen + 2];
    memset(longTitle, 'x', kTitleMaxLen + 1);
    longTitle[kTitleMaxLen + 1] = '\0';
    char longId[kIdMaxLen + 2];
    memset(longId, 'x', kIdMaxLen + 1);
    longId[kIdMaxLen + 1] = '\0';

    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(confirmMessage(""), 0));
    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(confirmMessage(nullptr), 0));
    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(confirmMessage(longValue), 0));
    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(confirmMessage("v", longId), 0));
    NewMessage titleTooLong{nullptr, longTitle, "v", FontSize::Small, Color::White, Kind::Confirm, 0};
    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(titleTooLong, 0));
    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(timedMessage("v", 0), 0));
    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(timedMessage("v", kMaxDurationS + 1), 0));
    NewMessage badColor{nullptr, "t", "v", FontSize::Small, static_cast<Color>(4), Kind::Confirm, 0};
    TEST_ASSERT_EQUAL(AddResult::Invalid, queue->add(badColor, 0));
    TEST_ASSERT_TRUE(queue->empty());
}

static void test_accepts_maximum_lengths()
{
    char value[kValueMaxLen + 1];
    memset(value, 'x', kValueMaxLen);
    value[kValueMaxLen] = '\0';
    TEST_ASSERT_EQUAL(AddResult::Added, queue->add(confirmMessage(value), 0));
    TEST_ASSERT_EQUAL(AddResult::Added, queue->add(timedMessage("v", kMaxDurationS), 0));
}

static void test_newest_first_and_jumps_to_new()
{
    fill(3);
    queue->scrollNext();
    TEST_ASSERT_EQUAL_STRING("m1", queue->current()->value);

    queue->add(confirmMessage("new"), 0);
    TEST_ASSERT_EQUAL(0, queue->cursor());
    TEST_ASSERT_EQUAL_STRING("new", queue->at(0).value);
    TEST_ASSERT_EQUAL_STRING("m0", queue->at(3).value);
}

static void test_scroll_wraps()
{
    fill(3);  // order: m2, m1, m0
    TEST_ASSERT_EQUAL_STRING("m2", queue->current()->value);
    queue->scrollNext();
    queue->scrollNext();
    TEST_ASSERT_EQUAL_STRING("m0", queue->current()->value);
    queue->scrollNext();
    TEST_ASSERT_EQUAL_STRING("m2", queue->current()->value);
}

static void test_full_queue_drops_new_messages()
{
    fill(kCapacity);
    TEST_ASSERT_TRUE(queue->full());
    TEST_ASSERT_EQUAL(AddResult::Full, queue->add(confirmMessage("dropped"), 0));
    TEST_ASSERT_EQUAL(kCapacity, queue->size());
    TEST_ASSERT_EQUAL_STRING("m29", queue->at(0).value);
}

static void test_same_id_replaces_and_moves_to_front()
{
    queue->add(confirmMessage("old", "status"), 0);
    queue->add(confirmMessage("other"), 0);
    TEST_ASSERT_EQUAL(AddResult::Replaced, queue->add(confirmMessage("new", "status"), 0));
    TEST_ASSERT_EQUAL(2, queue->size());
    TEST_ASSERT_EQUAL_STRING("new", queue->at(0).value);
    TEST_ASSERT_EQUAL_STRING("other", queue->at(1).value);
}

static void test_replace_allowed_when_full()
{
    queue->add(confirmMessage("old", "status"), 0);
    fill(kCapacity - 1);
    TEST_ASSERT_EQUAL(AddResult::Replaced, queue->add(confirmMessage("new", "status"), 0));
    TEST_ASSERT_EQUAL(kCapacity, queue->size());
    TEST_ASSERT_EQUAL_STRING("new", queue->at(0).value);
}

static void test_messages_without_id_never_replace()
{
    queue->add(confirmMessage("a"), 0);
    TEST_ASSERT_EQUAL(AddResult::Added, queue->add(confirmMessage("b", ""), 0));
    TEST_ASSERT_EQUAL(2, queue->size());
}

static void test_delete_current_shows_next_older()
{
    fill(3);  // m2, m1, m0
    queue->scrollNext();
    TEST_ASSERT_TRUE(queue->deleteCurrent());
    TEST_ASSERT_EQUAL(2, queue->size());
    TEST_ASSERT_EQUAL_STRING("m0", queue->current()->value);
}

static void test_delete_oldest_wraps_to_newest()
{
    fill(3);
    queue->scrollNext();
    queue->scrollNext();
    queue->deleteCurrent();
    TEST_ASSERT_EQUAL_STRING("m2", queue->current()->value);
}

static void test_delete_removes_timed_message()
{
    queue->add(timedMessage("t", 10), 0);
    TEST_ASSERT_TRUE(queue->deleteCurrent());
    TEST_ASSERT_TRUE(queue->empty());
}

static void test_clear_removes_all()
{
    fill(5);
    queue->clear();
    TEST_ASSERT_TRUE(queue->empty());
    TEST_ASSERT_NULL(queue->current());
}

static void test_expire_removes_only_due_timed_messages()
{
    queue->add(timedMessage("short", 1), 0);
    queue->add(confirmMessage("confirm"), 0);
    queue->add(timedMessage("long", 10), 0);

    TEST_ASSERT_FALSE(queue->expire(999));
    TEST_ASSERT_TRUE(queue->expire(1000));
    TEST_ASSERT_EQUAL(2, queue->size());
    TEST_ASSERT_TRUE(queue->expire(10000));
    TEST_ASSERT_EQUAL(1, queue->size());
    TEST_ASSERT_EQUAL_STRING("confirm", queue->current()->value);
    TEST_ASSERT_FALSE(queue->expire(UINT64_MAX));
}

static void test_expire_keeps_cursor_on_shown_message()
{
    queue->add(timedMessage("expires", 1), 0);  // will be oldest
    queue->add(confirmMessage("b"), 0);
    queue->add(confirmMessage("a"), 0);  // a, b, expires
    queue->scrollNext();
    TEST_ASSERT_EQUAL_STRING("b", queue->current()->value);
    queue->expire(1000);
    TEST_ASSERT_EQUAL_STRING("b", queue->current()->value);
}

static void test_expire_before_cursor_keeps_shown_message()
{
    queue->add(confirmMessage("old"), 0);
    queue->add(timedMessage("expires", 1), 0);  // expires, old
    queue->scrollNext();
    TEST_ASSERT_EQUAL_STRING("old", queue->current()->value);
    queue->expire(1000);
    TEST_ASSERT_EQUAL_STRING("old", queue->current()->value);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_empty);
    RUN_TEST(test_add_copies_fields);
    RUN_TEST(test_null_id_and_title_are_empty);
    RUN_TEST(test_rejects_invalid_input);
    RUN_TEST(test_accepts_maximum_lengths);
    RUN_TEST(test_newest_first_and_jumps_to_new);
    RUN_TEST(test_scroll_wraps);
    RUN_TEST(test_full_queue_drops_new_messages);
    RUN_TEST(test_same_id_replaces_and_moves_to_front);
    RUN_TEST(test_replace_allowed_when_full);
    RUN_TEST(test_messages_without_id_never_replace);
    RUN_TEST(test_delete_current_shows_next_older);
    RUN_TEST(test_delete_oldest_wraps_to_newest);
    RUN_TEST(test_delete_removes_timed_message);
    RUN_TEST(test_clear_removes_all);
    RUN_TEST(test_expire_removes_only_due_timed_messages);
    RUN_TEST(test_expire_keeps_cursor_on_shown_message);
    RUN_TEST(test_expire_before_cursor_keeps_shown_message);
    return UNITY_END();
}
