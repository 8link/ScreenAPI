#include "screen.h"

#include <TFT_eSPI.h>
#include <countdown.h>
#include <markup.h>
#include <qrcode.h>
#include <scroll_offset.h>
#include <string.h>
#include <text_wrap.h>

namespace screen {

namespace {

// Landscape layout, 240 x 135: top bar, title row, value area.
constexpr int kWidth = 240;
constexpr int kHeight = 135;
constexpr int kMargin = 4;
constexpr int kBarHeight = 20;
constexpr int kTitleTop = 23;
constexpr int kTitleHeight = 16;
constexpr int kValueTop = 43;
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
constexpr uint32_t kPopupMs = 3000;

constexpr uint16_t kBlue = 0x4C9F;  // lighter than TFT_BLUE, which is hard to read on black

// Top bar colors (D-028), chosen to survive the 8-bit frame buffer unchanged:
// each is the exact RGB565 form of an RGB332 color.
constexpr uint16_t kBarBackground = 0x210A;  // dark slate (36, 36, 85)
constexpr uint16_t kPillBackground = 0x420A;  // slate grey (73, 73, 85)
constexpr uint16_t kQueueAccent = 0xDC80;     // amber (219, 146, 0)
constexpr int kPillHeight = 18;
constexpr int kPillGap = 3;
constexpr int kPillPadding = 4;

// Markup tag names in mq::Color order (D-021).
const char* const kColorNames[] = {"white", "blue", "green", "red"};
static_assert(static_cast<int>(mq::Color::White) == 0 && static_cast<int>(mq::Color::Blue) == 1 &&
                  static_cast<int>(mq::Color::Green) == 2 && static_cast<int>(mq::Color::Red) == 3,
              "kColorNames must follow mq::Color order");

// Worst case: every character of the value is '\n'.
constexpr size_t kMaxLines = mq::kValueMaxLen + 1;

TFT_eSPI tft;
TFT_eSprite frame(&tft);

// Layout of the message on screen; rebuilt only when its text, font, or color
// changes, so scrolling does not restart when other messages change.
struct Shown {
    bool valid;
    char title[mq::kTitleMaxLen + 1];
    char value[mq::kValueMaxLen + 1];  // as received, with markup
    char text[mq::kValueMaxLen + 1];   // visible text, markup removed
    uint8_t colors[mq::kValueMaxLen + 1];  // mq::Color per visible character
    mq::FontSize fontSize;
    mq::Color color;
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
StatusBar lastBar;
bool lastBarValid = false;
uint64_t popupUntilMs = 0;
bool popupShown = false;

// QR code modules, copied out of the ESP-IDF encoder's callback.
constexpr int kQrMaxModules = 37;  // version 5; the provisioning payload needs version 4
bool qrModules[kQrMaxModules * kQrMaxModules];
int qrSize = 0;

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
    return shown.valid && shown.fontSize == message.fontSize && shown.color == message.color &&
           strcmp(shown.title, message.title) == 0 && strcmp(shown.value, message.value) == 0;
}

void layout(const mq::Message& message, uint64_t nowMs)
{
    strcpy(shown.title, message.title);
    strcpy(shown.value, message.value);
    shown.fontSize = message.fontSize;
    shown.color = message.color;
    ui::parseMarkup(shown.value, static_cast<uint8_t>(message.color), kColorNames, 4, shown.text, shown.colors);
    uint8_t font = fontFor(message.fontSize);
    shown.titleWidth = frame.textWidth(shown.title, kTitleFont);
    shown.lineHeight = frame.fontHeight(font);
    shown.lineCount = ui::wrapText(shown.text, kTextWidth, measureText, &font, lines, kMaxLines);
    shown.sinceMs = nowMs;
    shown.valid = true;
}

bool isScrolling()
{
    return shown.valid &&
           (shown.titleWidth > kTextWidth || static_cast<int>(shown.lineCount) * shown.lineHeight > kValueHeight);
}

// Draws one wrapped line as runs of equal color, left to right.
void drawLine(const ui::Line& line, int y, uint8_t font)
{
    int x = kMargin;
    size_t pos = line.start;
    const size_t end = line.start + line.length;
    while (pos < end) {
        const uint8_t color = shown.colors[pos];
        size_t runEnd = pos + 1;
        while (runEnd < end && shown.colors[runEnd] == color) {
            runEnd++;
        }
        frame.setTextColor(colorFor(static_cast<mq::Color>(color)));
        x += frame.drawString(copyToLineBuffer(shown.text + pos, runEnd - pos), x, y, font);
        pos = runEnd;
    }
}

void drawValue(const mq::Message& message, uint64_t elapsedMs)
{
    const uint8_t font = fontFor(message.fontSize);
    const int contentHeight = static_cast<int>(shown.lineCount) * shown.lineHeight;
    const int offset = ui::scrollOffset(contentHeight, kValueHeight, elapsedMs, kValueSpeedPxPerS, kScrollPauseMs);

    frame.setTextDatum(TL_DATUM);
    for (size_t i = 0; i < shown.lineCount; i++) {
        const int y = kValueTop + static_cast<int>(i) * shown.lineHeight - offset;
        if (y + shown.lineHeight <= kValueTop) {
            continue;
        }
        if (y >= kHeight) {
            break;
        }
        drawLine(lines[i], y, font);
    }
}

int32_t countdownFor(const mq::Message* message)
{
    if (message == nullptr || message->kind != mq::Kind::Timed) {
        return -1;
    }
    return static_cast<int32_t>(ui::countdownSeconds(message->remainingMs));
}

// Small box at the bottom right of the value area, drawn over the value text.
void drawCountdown(uint32_t seconds)
{
    char text[12];
    ui::formatCountdown(seconds, text, sizeof(text));
    const int width = frame.textWidth(text, kSmallFont) + 2 * kCountdownPadding;
    const int x = kWidth - width - 2;
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

// Rounded background for one top bar element.
void drawPill(int x, int width, uint16_t color)
{
    frame.fillRoundRect(x, 1, width, kPillHeight, 4, color);
}

// Battery outline, 18 x 9 px. On battery: a fill level, green above 50 %, amber
// above 20 %, red below. On USB power: an amber lightning bolt. Unknown: empty.
void drawBatteryIcon(int x, const StatusBar& bar)
{
    constexpr int kBodyWidth = 16;
    constexpr int kBodyHeight = 9;
    const int y = 1 + (kPillHeight - kBodyHeight) / 2;
    frame.drawRect(x, y, kBodyWidth, kBodyHeight, TFT_LIGHTGREY);
    frame.fillRect(x + kBodyWidth, y + 3, 2, kBodyHeight - 6, TFT_LIGHTGREY);
    if (!bar.batteryKnown) {
        return;
    }
    if (bar.externalPower) {
        const int cx = x + kBodyWidth / 2;
        frame.drawLine(cx + 2, y + 1, cx - 2, y + 4, kQueueAccent);
        frame.drawLine(cx - 2, y + 4, cx + 2, y + 4, kQueueAccent);
        frame.drawLine(cx + 2, y + 4, cx - 2, y + 7, kQueueAccent);
        return;
    }
    const uint16_t fill = bar.batteryPercent > 50 ? TFT_GREEN : bar.batteryPercent > 20 ? kQueueAccent : TFT_RED;
    const int fillWidth = (kBodyWidth - 4) * bar.batteryPercent / 100;
    if (fillWidth > 0) {
        frame.fillRect(x + 2, y + 2, fillWidth, kBodyHeight - 4, fill);
    }
}

uint16_t linkColor(Link link)
{
    switch (link) {
    case Link::Up:
        return TFT_GREEN;
    case Link::Pending:
        return kQueueAccent;
    case Link::Down:
        break;
    }
    return TFT_RED;
}

// Dark bar with separate pills, laid out right to left: clock, battery icon,
// queue position (amber, bold), then the network pill fills the space on the
// left, with a status stripe on its left edge (D-028).
void drawTopBar(const mq::MessageQueue& queue, const StatusBar& bar)
{
    constexpr int kCenterY = 1 + kPillHeight / 2;
    frame.fillRect(0, 0, kWidth, kBarHeight, kBarBackground);

    // Clock
    const int clockWidth = frame.textWidth("88:88", kBarFont) + 2 * kPillPadding;
    const int clockX = kWidth - 2 - clockWidth;
    drawPill(clockX, clockWidth, kPillBackground);
    frame.setTextColor(TFT_WHITE);
    frame.setTextDatum(MC_DATUM);
    frame.drawString(bar.clock, clockX + clockWidth / 2, kCenterY, kBarFont);

    // Battery
    constexpr int kIconWidth = 18;
    const int batteryWidth = kIconWidth + 2 * kPillPadding;
    const int batteryX = clockX - kPillGap - batteryWidth;
    drawPill(batteryX, batteryWidth, kPillBackground);
    drawBatteryIcon(batteryX + kPillPadding, bar);

    // Queue position: amber pill, black text drawn twice one pixel apart for a bold look
    char position[12];
    const size_t current = queue.empty() ? 0 : queue.cursor() + 1;
    snprintf(position, sizeof(position), "%u/%u", static_cast<unsigned>(current), static_cast<unsigned>(queue.size()));
    const int queueWidth = frame.textWidth(position, kBarFont) + 1 + 2 * kPillPadding;
    const int queueX = batteryX - kPillGap - queueWidth;
    drawPill(queueX, queueWidth, kQueueAccent);
    frame.setTextColor(TFT_BLACK);
    frame.drawString(position, queueX + queueWidth / 2, kCenterY, kBarFont);
    frame.drawString(position, queueX + queueWidth / 2 + 1, kCenterY, kBarFont);

    // Network: status stripe, then the label in the remaining space
    constexpr int kStripeWidth = 3;
    const int networkWidth = queueX - kPillGap - 2;
    drawPill(2, networkWidth, kPillBackground);
    frame.fillRect(2 + 2, 4, kStripeWidth, kPillHeight - 6, linkColor(bar.link));
    frame.setTextColor(TFT_LIGHTGREY);
    frame.setTextDatum(ML_DATUM);
    frame.drawString(bar.network, 2 + 2 + kStripeWidth + 3, kCenterY, kBarFont);
}

void drawTitle(uint64_t elapsedMs)
{
    const int offset = ui::scrollOffset(shown.titleWidth, kTextWidth, elapsedMs, kTitleSpeedPxPerS, kScrollPauseMs);
    frame.setTextDatum(TL_DATUM);
    frame.setTextColor(TFT_LIGHTGREY);
    frame.drawString(shown.title, kMargin - offset, kTitleTop, kTitleFont);
    frame.drawFastHLine(0, kValueTop - 2, kWidth, TFT_DARKGREY);
}

void drawQueueFullPopup()
{
    constexpr int kPopupWidth = 200;
    constexpr int kPopupHeight = 50;
    const int x = (kWidth - kPopupWidth) / 2;
    const int y = (kHeight - kPopupHeight) / 2;
    frame.fillRoundRect(x, y, kPopupWidth, kPopupHeight, 5, TFT_BLACK);
    frame.drawRoundRect(x, y, kPopupWidth, kPopupHeight, 5, TFT_RED);
    frame.drawRoundRect(x + 1, y + 1, kPopupWidth - 2, kPopupHeight - 2, 4, TFT_RED);
    frame.setTextDatum(MC_DATUM);
    frame.setTextColor(TFT_RED);
    frame.drawString("Queue full", kWidth / 2, y + 16, kSmallFont);
    frame.setTextColor(TFT_LIGHTGREY);
    frame.drawString("new messages dropped", kWidth / 2, y + 34, kSmallFont);
}

void render(const mq::MessageQueue& queue, const mq::Message* message, int32_t countdownS, uint64_t nowMs,
            const StatusBar& bar, bool popup)
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
    drawTopBar(queue, bar);
    if (message != nullptr) {
        drawTitle(nowMs - shown.sinceMs);
    }
    if (popup) {
        drawQueueFullPopup();
    }
    frame.pushSprite(0, 0);
}

void storeQr(esp_qrcode_handle_t qrcode)
{
    qrSize = esp_qrcode_get_size(qrcode);
    if (qrSize > kQrMaxModules) {
        qrSize = 0;
        return;
    }
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            qrModules[y * qrSize + x] = esp_qrcode_get_module(qrcode, x, y);
        }
    }
}

// Draws the QR code black on white with a 2-module quiet zone; returns its width.
int drawQr(int x0, int y0, int scale)
{
    constexpr int kQuiet = 2;
    const int total = (qrSize + 2 * kQuiet) * scale;
    frame.fillRect(x0, y0, total, total, TFT_WHITE);
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (qrModules[y * qrSize + x]) {
                frame.fillRect(x0 + (x + kQuiet) * scale, y0 + (y + kQuiet) * scale, scale, scale, TFT_BLACK);
            }
        }
    }
    return total;
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

void update(const mq::MessageQueue& queue, uint64_t nowMs, bool queueChanged, const StatusBar& bar)
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
    const bool barChanged = !lastBarValid || strcmp(bar.network, lastBar.network) != 0 || bar.link != lastBar.link ||
                            strcmp(bar.clock, lastBar.clock) != 0 || bar.batteryKnown != lastBar.batteryKnown ||
                            bar.externalPower != lastBar.externalPower || bar.batteryPercent != lastBar.batteryPercent;
    const bool popup = nowMs < popupUntilMs;
    if (!queueChanged && !relayout && !frameDue && countdownS == lastCountdownS && !barChanged &&
        popup == popupShown) {
        return;
    }
    render(queue, message, countdownS, nowMs, bar, popup);
    popupShown = popup;
    lastFrameMs = nowMs;
    lastCountdownS = countdownS;
    lastBar = bar;
    lastBarValid = true;
}

void showSetup(const char* qrPayload, const char* serviceName, const char* pop, const char* status, uint16_t statusColor)
{
    esp_qrcode_config_t config;
    config.display_func = storeQr;
    config.max_qrcode_version = 5;
    config.qrcode_ecc_level = ESP_QRCODE_ECC_LOW;
    qrSize = 0;
    esp_qrcode_generate(&config, qrPayload);

    frame.fillSprite(TFT_BLACK);
    int textX = kMargin;
    if (qrSize > 0) {
        constexpr int kScale = 3;  // version 4: (33 + 4) * 3 = 111 px
        const int size = (qrSize + 4) * kScale;
        textX += drawQr(kMargin, (kHeight - size) / 2, kScale) + 6;
    }

    char code[24];
    snprintf(code, sizeof(code), "Code %s", pop);
    frame.setTextDatum(TL_DATUM);
    frame.setTextColor(TFT_WHITE);
    frame.drawString("Wi-Fi setup", textX, 6, kSmallFont);
    frame.setTextColor(TFT_LIGHTGREY);
    frame.drawString("ESP BLE", textX, 28, kSmallFont);
    frame.drawString("Provisioning app", textX, 44, kSmallFont);
    frame.setTextColor(TFT_WHITE);
    frame.drawString(serviceName, textX, 66, kSmallFont);
    frame.drawString(code, textX, 82, kSmallFont);
    frame.setTextColor(statusColor);
    frame.drawString(status, textX, 110, kSmallFont);
    frame.pushSprite(0, 0);
}

void showWelcome(const char* ssid, const char* ip)
{
    char connected[48];
    snprintf(connected, sizeof(connected), "Connected to %s", ssid);
    frame.fillSprite(TFT_BLACK);
    frame.setTextDatum(TC_DATUM);
    frame.setTextColor(TFT_LIGHTGREY);
    frame.drawString("ScreenAPI v" FW_VERSION, kWidth / 2, 8, kSmallFont);
    frame.setTextColor(TFT_WHITE);
    frame.drawString(connected, kWidth / 2, 34, kSmallFont);
    frame.setTextColor(TFT_GREEN);
    frame.drawString(ip, kWidth / 2, 58, kLargeFont);
    frame.setTextColor(TFT_LIGHTGREY);
    frame.drawString("screenapi.local/mcp", kWidth / 2, 104, kSmallFont);
    frame.pushSprite(0, 0);
}

void showQueueFullPopup(uint64_t nowMs)
{
    popupUntilMs = nowMs + kPopupMs;
}

void sendScreenshot(Print& out)
{
    const uint8_t* pixels = static_cast<const uint8_t*>(frame.getPointer());
    if (pixels == nullptr) {
        out.println("SCREENSHOT unavailable");
        return;
    }
    static const char kHex[] = "0123456789abcdef";
    char row[kWidth * 2 + 1];
    out.printf("SCREENSHOT %d %d rgb332\n", kWidth, kHeight);
    for (int y = 0; y < kHeight; y++) {
        for (int x = 0; x < kWidth; x++) {
            const uint8_t value = pixels[y * kWidth + x];
            row[2 * x] = kHex[value >> 4];
            row[2 * x + 1] = kHex[value & 0x0F];
        }
        row[kWidth * 2] = '\0';
        out.println(row);
    }
    out.println("END");
}

void showNotice(const char* text)
{
    frame.fillSprite(TFT_BLACK);
    frame.setTextDatum(MC_DATUM);
    frame.setTextColor(TFT_WHITE);
    frame.drawString(text, kWidth / 2, kHeight / 2, kSmallFont);
    frame.pushSprite(0, 0);
}

}  // namespace screen
