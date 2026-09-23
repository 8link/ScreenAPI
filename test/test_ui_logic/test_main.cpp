#include <battery_level.h>
#include <button_pair.h>
#include <button_tracker.h>
#include <countdown.h>
#include <markup.h>
#include <scroll_offset.h>
#include <string.h>
#include <text_wrap.h>
#include <unity.h>

using namespace ui;

// Every character is 1 pixel wide, so widths equal character counts.
static int measureChars(const char*, size_t length, void*)
{
    return static_cast<int>(length);
}

static Line lines[32];

static size_t wrap(const char* text, int width)
{
    return wrapText(text, width, measureChars, nullptr, lines, 32);
}

static void assertLine(const char* text, size_t index, const char* expected)
{
    char actual[64] = {};
    memcpy(actual, text + lines[index].start, lines[index].length);
    TEST_ASSERT_EQUAL_STRING(expected, actual);
}

void setUp() {}
void tearDown() {}

static void test_wrap_short_text_is_one_line()
{
    const char* text = "hello world";
    TEST_ASSERT_EQUAL(1, wrap(text, 20));
    assertLine(text, 0, "hello world");
}

static void test_wrap_breaks_at_spaces()
{
    const char* text = "one two three four";
    TEST_ASSERT_EQUAL(3, wrap(text, 8));
    assertLine(text, 0, "one two");
    assertLine(text, 1, "three");
    assertLine(text, 2, "four");
}

static void test_wrap_fits_exact_width()
{
    const char* text = "abcd efgh";
    TEST_ASSERT_EQUAL(2, wrap(text, 4));
    assertLine(text, 0, "abcd");
    assertLine(text, 1, "efgh");
}

static void test_wrap_breaks_long_word()
{
    const char* text = "abcdefghij x";
    TEST_ASSERT_EQUAL(3, wrap(text, 4));
    assertLine(text, 0, "abcd");
    assertLine(text, 1, "efgh");
    assertLine(text, 2, "ij x");  // the rest of a broken word can share a line
}

static void test_wrap_honors_newlines_and_empty_lines()
{
    const char* text = "ab\n\ncd";
    TEST_ASSERT_EQUAL(3, wrap(text, 10));
    assertLine(text, 0, "ab");
    assertLine(text, 1, "");
    assertLine(text, 2, "cd");
}

static void test_wrap_drops_spaces_at_wrap_point()
{
    const char* text = "abc   def";
    TEST_ASSERT_EQUAL(2, wrap(text, 4));
    assertLine(text, 0, "abc");
    assertLine(text, 1, "def");
}

static void test_wrap_respects_max_lines()
{
    TEST_ASSERT_EQUAL(2, wrapText("a b c d", 1, measureChars, nullptr, lines, 2));
}

static void test_wrap_width_below_one_char_still_progresses()
{
    const char* text = "ab";
    TEST_ASSERT_EQUAL(2, wrap(text, 0));
    assertLine(text, 0, "a");
    assertLine(text, 1, "b");
}

static void test_scroll_zero_when_content_fits()
{
    TEST_ASSERT_EQUAL(0, scrollOffset(100, 100, 5000, 20, 1000));
    TEST_ASSERT_EQUAL(0, scrollOffset(50, 100, 5000, 20, 1000));
    TEST_ASSERT_EQUAL(0, scrollOffset(200, 100, 5000, 0, 1000));
}

static void test_scroll_cycle()
{
    // distance 100 px at 50 px/s = 2000 ms moving; 1000 ms pause each end.
    TEST_ASSERT_EQUAL(0, scrollOffset(200, 100, 0, 50, 1000));
    TEST_ASSERT_EQUAL(0, scrollOffset(200, 100, 999, 50, 1000));
    TEST_ASSERT_EQUAL(25, scrollOffset(200, 100, 1500, 50, 1000));
    TEST_ASSERT_EQUAL(99, scrollOffset(200, 100, 2999, 50, 1000));
    TEST_ASSERT_EQUAL(100, scrollOffset(200, 100, 3000, 50, 1000));
    TEST_ASSERT_EQUAL(100, scrollOffset(200, 100, 3999, 50, 1000));
    TEST_ASSERT_EQUAL(0, scrollOffset(200, 100, 4000, 50, 1000));  // next cycle
    TEST_ASSERT_EQUAL(25, scrollOffset(200, 100, 5500, 50, 1000));
}

static void test_button_short_press_on_release()
{
    ButtonTracker button(30, 1500);
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(true, 0));
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(true, 30));
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(false, 200));
    TEST_ASSERT_EQUAL(ButtonEvent::Short, button.update(false, 230));
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(false, 300));
}

static void test_button_ignores_bounce()
{
    ButtonTracker button(30, 1500);
    button.update(true, 0);
    button.update(false, 10);
    button.update(true, 20);
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(false, 25));
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(false, 100));
}

static void test_button_long_press_fires_once_while_held()
{
    ButtonTracker button(30, 1500);
    button.update(true, 0);
    button.update(true, 30);
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(true, 1529));
    TEST_ASSERT_EQUAL(ButtonEvent::Long, button.update(true, 1530));
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(true, 5000));
    button.update(false, 5100);
    TEST_ASSERT_EQUAL(ButtonEvent::None, button.update(false, 5130));
}

static void test_button_works_again_after_long_press()
{
    ButtonTracker button(30, 1500);
    button.update(true, 0);
    button.update(true, 30);
    button.update(true, 1530);
    button.update(false, 2000);
    button.update(false, 2030);
    button.update(true, 3000);
    button.update(true, 3030);
    button.update(false, 3100);
    TEST_ASSERT_EQUAL(ButtonEvent::Short, button.update(false, 3130));
}

static void test_countdown_seconds_round_up()
{
    TEST_ASSERT_EQUAL_UINT32(0, countdownSeconds(0));
    TEST_ASSERT_EQUAL_UINT32(1, countdownSeconds(1));
    TEST_ASSERT_EQUAL_UINT32(1, countdownSeconds(1000));
    TEST_ASSERT_EQUAL_UINT32(2, countdownSeconds(1001));
    TEST_ASSERT_EQUAL_UINT32(86400, countdownSeconds(86400000));
}

static void test_countdown_format()
{
    char text[16];
    formatCountdown(0, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("0s", text);
    formatCountdown(59, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("59s", text);
    formatCountdown(60, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("1:00", text);
    formatCountdown(245, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("4:05", text);
    formatCountdown(3599, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("59:59", text);
    formatCountdown(3723, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("1:02:03", text);
    formatCountdown(86400, text, sizeof(text));
    TEST_ASSERT_EQUAL_STRING("24:00:00", text);
}

static const char* const kColorNames[] = {"white", "blue", "green", "red"};
static char markupText[64];
static uint8_t markupColors[64];

static size_t parse(const char* markup, uint8_t baseColor = 0)
{
    return parseMarkup(markup, baseColor, kColorNames, 4, markupText, markupColors);
}

static void assertColors(const char* expected)
{
    // expected holds one digit per visible character.
    TEST_ASSERT_EQUAL(strlen(expected), strlen(markupText));
    for (size_t i = 0; expected[i] != '\0'; i++) {
        TEST_ASSERT_EQUAL_UINT8(expected[i] - '0', markupColors[i]);
    }
}

static void test_markup_plain_text_uses_base_color()
{
    TEST_ASSERT_EQUAL(3, parse("abc", 2));
    TEST_ASSERT_EQUAL_STRING("abc", markupText);
    assertColors("222");
}

static void test_markup_color_and_reset()
{
    TEST_ASSERT_EQUAL(5, parse("a{red}bc{/}de"));
    TEST_ASSERT_EQUAL_STRING("abcde", markupText);
    assertColors("03300");
}

static void test_markup_reset_returns_to_base_color()
{
    parse("{green}a{/}b", 1);
    assertColors("21");
}

static void test_markup_adjacent_tags_and_tag_at_end()
{
    parse("{blue}{red}x{green}");
    TEST_ASSERT_EQUAL_STRING("x", markupText);
    assertColors("3");
}

static void test_markup_unknown_braces_are_literal()
{
    TEST_ASSERT_EQUAL(13, parse("{\"k\":1} {Red}"));
    TEST_ASSERT_EQUAL_STRING("{\"k\":1} {Red}", markupText);
    parse("{re}x{redd}");
    TEST_ASSERT_EQUAL_STRING("{re}x{redd}", markupText);
}

static void test_markup_unclosed_brace_is_literal()
{
    parse("a{red");
    TEST_ASSERT_EQUAL_STRING("a{red", markupText);
    parse("{");
    TEST_ASSERT_EQUAL_STRING("{", markupText);
}

static void test_markup_color_spans_newline()
{
    parse("{red}a\nb");
    TEST_ASSERT_EQUAL_STRING("a\nb", markupText);
    assertColors("333");
}

// Steps a pair through time in 5 ms loop ticks and returns the first non-None event.
static PairEvent run(ButtonPair& pair, bool first, bool second, uint64_t& now, uint64_t untilMs)
{
    PairEvent result = PairEvent::None;
    for (; now <= untilMs; now += 5) {
        const PairEvent event = pair.update(first, second, now);
        if (result == PairEvent::None) {
            result = event;
        }
    }
    return result;
}

static void test_pair_single_presses_pass_through()
{
    ButtonPair pair(30, 1500, 5000);
    uint64_t now = 0;
    run(pair, true, false, now, 100);
    TEST_ASSERT_EQUAL(PairEvent::FirstShort, run(pair, false, false, now, 200));
    run(pair, false, true, now, 300);
    TEST_ASSERT_EQUAL(PairEvent::SecondShort, run(pair, false, false, now, 400));
    TEST_ASSERT_EQUAL(PairEvent::FirstLong, run(pair, true, false, now, 2000));
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, false, false, now, 2200));
}

static void test_pair_both_held_fires_once_and_suppresses_singles()
{
    ButtonPair pair(30, 1500, 5000);
    uint64_t now = 0;
    // Hold both past the single long-press time: no FirstLong, no SecondLong.
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, true, true, now, 4990));
    TEST_ASSERT_EQUAL(PairEvent::BothLong, run(pair, true, true, now, 5000));
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, true, true, now, 9000));
    // Releasing both reports nothing.
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, false, false, now, 9500));
    // Afterwards single presses work again.
    run(pair, true, false, now, 9600);
    TEST_ASSERT_EQUAL(PairEvent::FirstShort, run(pair, false, false, now, 9700));
}

static void test_pair_short_both_press_reports_nothing()
{
    ButtonPair pair(30, 1500, 5000);
    uint64_t now = 0;
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, true, true, now, 300));
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, false, false, now, 600));
}

static void test_pair_staggered_release_reports_nothing()
{
    ButtonPair pair(30, 1500, 5000);
    uint64_t now = 0;
    run(pair, true, true, now, 1000);
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, false, true, now, 3000));  // second still held past 1.5 s
    TEST_ASSERT_EQUAL(PairEvent::None, run(pair, false, false, now, 3500));
}

static void test_pair_release_settling_after_a_long_pause()
{
    ButtonPair pair(30, 1500, 5000);
    uint64_t now = 0;
    run(pair, true, true, now, 500);
    pair.update(false, false, now);  // release seen
    now += 200;                      // loop blocked, for example by a flash write
    TEST_ASSERT_EQUAL(PairEvent::None, pair.update(false, false, now));
    now += 5;
    TEST_ASSERT_EQUAL(PairEvent::None, pair.update(false, false, now));
}

static void test_battery_level_external_power()
{
    TEST_ASSERT_TRUE(batteryLevel(4765).external);
    TEST_ASSERT_TRUE(batteryLevel(kExternalPowerMv).external);
    TEST_ASSERT_FALSE(batteryLevel(kExternalPowerMv - 1).external);
}

static void test_battery_level_curve()
{
    TEST_ASSERT_EQUAL(100, batteryLevel(4300).percent);
    TEST_ASSERT_EQUAL(100, batteryLevel(4200).percent);
    TEST_ASSERT_EQUAL(95, batteryLevel(4150).percent);
    TEST_ASSERT_EQUAL(62, batteryLevel(3900).percent);
    TEST_ASSERT_EQUAL(31, batteryLevel(3750).percent);
    TEST_ASSERT_EQUAL(0, batteryLevel(3300).percent);
    TEST_ASSERT_EQUAL(0, batteryLevel(2000).percent);
    TEST_ASSERT_EQUAL(0, batteryLevel(0).percent);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_wrap_short_text_is_one_line);
    RUN_TEST(test_wrap_breaks_at_spaces);
    RUN_TEST(test_wrap_fits_exact_width);
    RUN_TEST(test_wrap_breaks_long_word);
    RUN_TEST(test_wrap_honors_newlines_and_empty_lines);
    RUN_TEST(test_wrap_drops_spaces_at_wrap_point);
    RUN_TEST(test_wrap_respects_max_lines);
    RUN_TEST(test_wrap_width_below_one_char_still_progresses);
    RUN_TEST(test_scroll_zero_when_content_fits);
    RUN_TEST(test_scroll_cycle);
    RUN_TEST(test_button_short_press_on_release);
    RUN_TEST(test_button_ignores_bounce);
    RUN_TEST(test_button_long_press_fires_once_while_held);
    RUN_TEST(test_button_works_again_after_long_press);
    RUN_TEST(test_countdown_seconds_round_up);
    RUN_TEST(test_countdown_format);
    RUN_TEST(test_markup_plain_text_uses_base_color);
    RUN_TEST(test_markup_color_and_reset);
    RUN_TEST(test_markup_reset_returns_to_base_color);
    RUN_TEST(test_markup_adjacent_tags_and_tag_at_end);
    RUN_TEST(test_markup_unknown_braces_are_literal);
    RUN_TEST(test_markup_unclosed_brace_is_literal);
    RUN_TEST(test_markup_color_spans_newline);
    RUN_TEST(test_pair_single_presses_pass_through);
    RUN_TEST(test_pair_both_held_fires_once_and_suppresses_singles);
    RUN_TEST(test_pair_short_both_press_reports_nothing);
    RUN_TEST(test_pair_staggered_release_reports_nothing);
    RUN_TEST(test_pair_release_settling_after_a_long_pause);
    RUN_TEST(test_battery_level_external_power);
    RUN_TEST(test_battery_level_curve);
    return UNITY_END();
}
