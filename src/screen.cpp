#include "screen.h"

#include <TFT_eSPI.h>
#include <countdown.h>
#include <scroll_offset.h>
#include <string.h>
#include <text_wrap.h>

namespace screen {

namespace {

// Landscape layout, 240 x 135: top bar, title row, value area.
constexpr int kWidth = 240;
constexpr int kHeight = 135;
constexpr int kMargin = 4;
constexpr int kBarHeight = 18;
constexpr int kTitleTop = 21;
constexpr int kTitleHeight = 16;
constexpr int kValueTop = 41;
constexpr int kValueHeight = kHeight - kValueTop;
constexpr int kTextWidth = kWidth - 2 * kMargin;
constexpr int kCountdownHeight = 20;
constexpr int kCountdownPadding = 5;

constexpr uint8_t kBarFont = 2;
constexpr uint8_t kTitleFont = 2;
constexpr uint8_t kSmallFont = 2;
constexpr uint8_t kLargeFont = 4;

constexpr uint32_t kTitleSpeedPxPerS = 40;
constexpr uint32_t kValueSpeedPxPerS = 20;
constexpr uint32_t kScrollPauseMs = 1500;
constexpr uint32_t kFrameMs = 33;

constexpr uint16_t kBlue = 0x4C9F;  // lighter than TFT_BLUE, which is hard to read on black

// Worst case: every character of the value is '\n'.
constexpr size_t kMaxLines = mq::kValueMaxLen + 1;

TFT_eSPI tft;
TFT_eSprite frame(&tft);

// Layout of the message on screen; rebuilt only when its text or font changes,
// so scrolling does not restart when other messages change.
struct Shown {
    bool valid;
    char title[mq::kTitleMaxLen + 1];
    char value[mq::kValueMaxLen + 1];
    mq::FontSize fontSize;
    int titleWidth;
    int lineHeight;
    size_t lineCount;
    uint64_t sinceMs;
};

Shown shown;
ui::Line lines[kMaxLines];
char lineBuffer[mq::kValueMaxLen + 1];
uint64_t lastFrameMs = 0;
int32_t lastCountdownS = -1;  // -1: no countdown on screen

uint8_t fontFor(mq::FontSize size)
{
    return size == mq::FontSize::Large ? kLargeFont : kSmallFont;
}

uint16_t colorFor(mq::Color color)
{
    switch (color) {
    case mq::Color::Blue:
        return kBlue;
    case mq::Color::Green:
        return TFT_GREEN;
    case mq::Color::Red:
        return TFT_RED;
    case mq::Color::White:
        break;
    }
    return TFT_WHITE;
}

const char* copyToLineBuffer(const char* text, size_t length)
{
    memcpy(lineBuffer, text, length);
    lineBuffer[length] = '\0';
    return lineBuffer;
}

int measureText(const char* text, size_t length, void* context)
{
    return frame.textWidth(copyToLineBuffer(text, length), *static_cast<uint8_t*>(context));
}

bool isShown(const mq::Message& message)
{
    return shown.valid && shown.fontSize == message.fontSize && strcmp(shown.title, message.title) == 0 &&
           strcmp(shown.value, message.value) == 0;
}

void layout(const mq::Message& message, uint64_t nowMs)
{
    strcpy(shown.title, message.title);
    strcpy(shown.value, message.value);
    shown.fontSize = message.fontSize;
    uint8_t font = fontFor(message.fontSize);
    shown.titleWidth = frame.textWidth(shown.title, kTitleFont);
    shown.lineHeight = frame.fontHeight(font);
    shown.lineCount = ui::wrapText(shown.value, kTextWidth, measureText, &font, lines, kMaxLines);
    shown.sinceMs = nowMs;
    shown.valid = true;
}

bool isScrolling()
{
    return shown.valid &&
           (shown.titleWidth > kTextWidth || static_cast<int>(shown.lineCount) * shown.lineHeight > kValueHeight);
}

void drawValue(const mq::Message& message, uint64_t elapsedMs)
{
    const uint8_t font = fontFor(message.fontSize);
    const int contentHeight = static_cast<int>(shown.lineCount) * shown.lineHeight;
    const int offset = ui::scrollOffset(contentHeight, kValueHeight, elapsedMs, kValueSpeedPxPerS, kScrollPauseMs);

    frame.setTextDatum(TL_DATUM);
    frame.setTextColor(colorFor(message.color));
    for (size_t i = 0; i < shown.lineCount; i++) {
        const int y = kValueTop + static_cast<int>(i) * shown.lineHeight - offset;
        if (y + shown.lineHeight <= kValueTop) {
            continue;
        }
        if (y >= kHeight) {
            break;
        }
        frame.drawString(copyToLineBuffer(shown.value + lines[i].start, lines[i].length), kMargin, y, font);
    }
}

int32_t countdownFor(const mq::Message* message)
{
    if (message == nullptr || message->kind != mq::Kind::Timed) {
        return -1;
    }
    return static_cast<int32_t>(ui::countdownSeconds(message->remainingMs));
}

// Small box at the bottom left of the value area, drawn over the value text.
void drawCountdown(uint32_t seconds)
{
    char text[12];
    ui::formatCountdown(seconds, text, sizeof(text));
    const int width = frame.textWidth(text, kSmallFont) + 2 * kCountdownPadding;
    const int x = 2;
    const int y = kHeight - kCountdownHeight - 2;
    frame.fillRoundRect(x, y, width, kCountdownHeight, 3, TFT_BLACK);
    frame.drawRoundRect(x, y, width, kCountdownHeight, 3, TFT_DARKGREY);
    frame.setTextDatum(MC_DATUM);
    frame.setTextColor(TFT_LIGHTGREY);
    frame.drawString(text, x + width / 2, y + kCountdownHeight / 2, kSmallFont);
}

void drawEmpty()
{
    frame.setTextDatum(MC_DATUM);
    frame.setTextColor(TFT_DARKGREY);
    frame.drawString("No messages", kWidth / 2, kValueTop + kValueHeight / 2, kSmallFont);
}

// IP, battery, and clock are placeholders until F-002, F-007, and F-008 exist.
void drawTopBar(const mq::MessageQueue& queue)
{
    char position[12];
    const size_t current = queue.empty() ? 0 : queue.cursor() + 1;
    snprintf(position, sizeof(position), "%u/%u", static_cast<unsigned>(current), static_cast<unsigned>(queue.size()));

    frame.setTextColor(TFT_LIGHTGREY);
    frame.setTextDatum(TL_DATUM);
    frame.drawString("no network", kMargin, 1, kBarFont);
    frame.setTextDatum(TC_DATUM);
    frame.drawString(position, kWidth / 2, 1, kBarFont);
    frame.setTextDatum(TR_DATUM);
    frame.drawString("--%  --:--", kWidth - kMargin, 1, kBarFont);
    frame.drawFastHLine(0, kBarHeight, kWidth, TFT_DARKGREY);
}

void drawTitle(uint64_t elapsedMs)
{
    const int offset = ui::scrollOffset(shown.titleWidth, kTextWidth, elapsedMs, kTitleSpeedPxPerS, kScrollPauseMs);
    frame.setTextDatum(TL_DATUM);
    frame.setTextColor(TFT_LIGHTGREY);
    frame.drawString(shown.title, kMargin - offset, kTitleTop, kTitleFont);
    frame.drawFastHLine(0, kValueTop - 2, kWidth, TFT_DARKGREY);
}

void render(const mq::MessageQueue& queue, const mq::Message* message, int32_t countdownS, uint64_t nowMs)
{
    frame.fillSprite(TFT_BLACK);
    if (message != nullptr) {
        drawValue(*message, nowMs - shown.sinceMs);
        if (countdownS >= 0) {
            drawCountdown(static_cast<uint32_t>(countdownS));
        }
    } else {
        drawEmpty();
    }
    // The value scrolls under the header, so clear the header area before drawing it.
    frame.fillRect(0, 0, kWidth, kValueTop, TFT_BLACK);
    drawTopBar(queue);
    if (message != nullptr) {
        drawTitle(nowMs - shown.sinceMs);
    }
    frame.pushSprite(0, 0);
}

}  // namespace

bool begin()
{
    tft.init();  // also switches the backlight on (TFT_BL in tft_setup.h)
    tft.setRotation(1);  // landscape, 240 x 135
    tft.fillScreen(TFT_BLACK);
    // 8-bit color halves the buffer to 32,400 bytes; the four text colors survive the reduction.
    frame.setColorDepth(8);
    return frame.createSprite(kWidth, kHeight) != nullptr;
}

void showBootScreen()
{
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("ScreenAPI", kWidth / 2, kHeight / 2 - 14, 4);
    tft.drawString("v" FW_VERSION, kWidth / 2, kHeight / 2 + 16, 2);
}

void update(const mq::MessageQueue& queue, uint64_t nowMs, bool queueChanged)
{
    const mq::Message* message = queue.current();
    const bool relayout = message != nullptr ? !isShown(*message) : shown.valid;
    if (relayout) {
        if (message != nullptr) {
            layout(*message, nowMs);
        } else {
            shown.valid = false;
        }
    }

    const int32_t countdownS = countdownFor(message);
    const bool frameDue = isScrolling() && nowMs - lastFrameMs >= kFrameMs;
    if (!queueChanged && !relayout && !frameDue && countdownS == lastCountdownS) {
        return;
    }
    render(queue, message, countdownS, nowMs);
    lastFrameMs = nowMs;
    lastCountdownS = countdownS;
}

}  // namespace screen
