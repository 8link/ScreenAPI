#include <Arduino.h>
#include <button_tracker.h>
#include <esp_timer.h>
#include <message_queue.h>

#include "pins.h"
#include "screen.h"
#include "storage.h"

namespace {

constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kLongPressMs = 1500;  // hold delete this long to clear all
constexpr uint32_t kBootScreenMs = 1500;
constexpr uint32_t kSaveDelayMs = 1000;  // one flash write for a burst of changes

mq::MessageQueue queue;
ui::ButtonTracker deleteButton(kDebounceMs, kLongPressMs);
ui::ButtonTracker scrollButton(kDebounceMs, kLongPressMs);
bool saveDue = false;
uint64_t saveAtMs = 0;

// 64-bit uptime; millis() is 32-bit and wraps after about 49.7 days.
uint64_t nowMs()
{
    return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
}

// Temporary sample messages until messages arrive over MCP (PROJECT.md F-011).
// The last one added is shown first.
void addDemoMessages()
{
    const mq::NewMessage demos[] = {
        {"demo-timer", "Timer",
         "Timed message: it counts down only while it is on screen. The box at the bottom right shows the time left.",
         mq::FontSize::Small, mq::Color::White, mq::Kind::Timed, 60},
        {"demo-error", "Upload failed",
         "Serial port not found. Check the USB cable and the USB-UART driver, then run the upload again.",
         mq::FontSize::Large, mq::Color::Red, mq::Kind::Confirm, 0},
        {"demo-colors", "Colors",
         "Parts of the text can have their own color: {green}green{/}, {red}red{/}, {blue}blue{/}, and back to "
         "the message color. Unknown tags like {yellow} stay as written.",
         mq::FontSize::Small, mq::Color::White, mq::Kind::Confirm, 0},
        {"demo-long", "Claude Code - ScreenAPI - working on message screen and buttons",
         "Demo of vertical scrolling. This value is longer than the text area, so it pauses at the top, "
         "scrolls down slowly, pauses at the bottom, and then jumps back to the top. The title above scrolls "
         "sideways the same way when it is wider than the screen. Press the delete button to remove this "
         "message, hold it for 1.5 seconds to clear all messages, and press the scroll button to show the "
         "next message.",
         mq::FontSize::Small, mq::Color::White, mq::Kind::Confirm, 0},
        {"demo-tests", "Tests", "{green}45 passed{/}, {red}0 failed{/}. Gone after 30 s on screen.",
         mq::FontSize::Large, mq::Color::White, mq::Kind::Timed, 30},
    };
    for (const mq::NewMessage& demo : demos) {
        if (queue.add(demo) != mq::AddResult::Added) {
            Serial.printf("Demo message rejected: %s\n", demo.id);
        }
    }
}

// Returns true if the screen needs a redraw. Sets contentChanged when messages
// were removed; scrolling alone is not saved, so it does not set it.
bool handleButtons(uint64_t now, bool& contentChanged)
{
    bool changed = false;
    switch (deleteButton.update(digitalRead(PIN_BUTTON_DELETE) == LOW, now)) {
    case ui::ButtonEvent::Short:
        changed = queue.deleteCurrent();
        contentChanged |= changed;
        if (changed) {
            Serial.printf("Deleted message, %u left\n", static_cast<unsigned>(queue.size()));
        }
        break;
    case ui::ButtonEvent::Long:
        if (!queue.empty()) {
            queue.clear();
            changed = true;
            contentChanged = true;
            Serial.println("Cleared all messages");
        }
        break;
    case ui::ButtonEvent::None:
        break;
    }

    // The scroll button has no long-press action.
    if (scrollButton.update(digitalRead(PIN_BUTTON_SCROLL) == LOW, now) == ui::ButtonEvent::Short &&
        queue.size() > 1) {
        queue.scrollNext();
        changed = true;
        Serial.printf("Showing message %u of %u\n", static_cast<unsigned>(queue.cursor() + 1),
                      static_cast<unsigned>(queue.size()));
    }
    return changed;
}

void scheduleSave(uint64_t now)
{
    saveDue = true;
    saveAtMs = now + kSaveDelayMs;
}

void saveIfDue(uint64_t now)
{
    if (!saveDue || now < saveAtMs) {
        return;
    }
    saveDue = false;
    const uint64_t startMs = nowMs();
    if (storage::save(queue)) {
        Serial.printf("Storage: save took %u ms\n", static_cast<unsigned>(nowMs() - startMs));
    }
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    Serial.printf("ScreenAPI v%s\n", FW_VERSION);

    // GPIO35 is input-only and has no internal pull-up; the board provides an external one.
    pinMode(PIN_BUTTON_DELETE, INPUT);
    pinMode(PIN_BUTTON_SCROLL, INPUT_PULLUP);

    if (!screen::begin()) {
        Serial.println("Screen buffer allocation failed");
    }
    screen::showBootScreen();
    delay(kBootScreenMs);

    storage::begin();
    const size_t restored = storage::load(queue);
    Serial.printf("Restored %u messages\n", static_cast<unsigned>(restored));
    if (restored == 0) {
        addDemoMessages();
        scheduleSave(nowMs());
    }
    Serial.printf("Queue: %u messages\n", static_cast<unsigned>(queue.size()));
    Serial.printf("Free heap: %u bytes, largest block: %u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()));
    screen::update(queue, nowMs(), true);
}

void loop()
{
    const uint64_t now = nowMs();
    // Tick before handling buttons, so time already spent is charged to the message that was on screen.
    bool contentChanged = queue.tick(now);
    if (contentChanged) {
        Serial.printf("Timed message expired, %u left\n", static_cast<unsigned>(queue.size()));
    }
    const bool changed = handleButtons(now, contentChanged) || contentChanged;
    if (contentChanged) {
        scheduleSave(now);
    }
    saveIfDue(now);
    screen::update(queue, now, changed);
    delay(5);
}
