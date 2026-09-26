#include "screen.h"

#include <countdown.h>
#include <markup.h>
#include <math.h>
#include <qrcode.h>
#include <scroll_offset.h>
#include <string.h>
#include <text_wrap.h>

#include "board.h"
#include "hal.h"

namespace screen {

namespace {

// Layout: top bar, title row, value area. Widths derive from the board's screen
// size, row heights from the board's fonts (computed in begin()).
constexpr int kWidth = board::kScreenWidth;
constexpr int kHeight = board::kScreenHeight;
static_assert(kWidth >= 240 && kHeight >= 135, "layout needs at least 240 x 135");
constexpr int kMargin = 4;
constexpr int kTextWidth = kWidth - 2 * kMargin;
constexpr int kCountdownPadding = 5;
constexpr int kPillGap = 3;
constexpr int kPillPadding = 4;

// Rounded display corners (D-036). The corner test measures the diagonal inset
// d at which content is complete; a circular corner of radius R reaches the
// diagonal at R * (1 - 1/sqrt(2)), so R is at most d / 0.293. Layout uses that
// upper bound so nothing is cut: on rounded boards the top bar sits high with
// capsule pills and enough room at the sides, on black instead of a bar color.
constexpr bool kRoundedCorners = board::kCornerInset > 0;
constexpr int kCornerRadiusMax = (board::kCornerInset * 1000 + 292) / 293;
constexpr int kRoundedBarTop = 8;

constexpr uint32_t kTitleSpeedPxPerS = 40;
constexpr uint32_t kValueSpeedPxPerS = 14;
constexpr uint32_t kArrivalSpeedPxPerS = 20;
constexpr uint32_t kScrollPauseMs = 1500;
constexpr uint32_t kFrameMs = 33;
constexpr uint32_t kPopupMs = 3000;

constexpr uint16_t kBlack = 0x0000;
constexpr uint16_t kWhite = 0xFFFF;
constexpr uint16_t kDarkGrey = 0x7BEF;
constexpr uint16_t kBlue = 0x4C9F;  // lighter than pure blue, which is hard to read on black
constexpr uint16_t kTextGrey = 0xC618;   // message text, "white" in markup (198, 195, 198)
constexpr uint16_t kTitleRule = 0x4208;  // grey line under the title (66, 66, 66)

// Top bar colors (D-028).
constexpr uint16_t kBarBackground = 0x210A;  // dark slate (36, 36, 85)
constexpr uint16_t kPillBackground = 0x420A;  // slate grey (73, 73, 85)
constexpr uint16_t kQueueAccent = 0xDC80;     // amber (219, 146, 0)
constexpr uint16_t kBandBackground = 0x2966;  // cool dark grey (40, 44, 48), rounded boards
constexpr uint16_t kBandEdge = 0x4208;        // grey line under the band (66, 66, 66)

// Markup tag names in mq::Color order (D-021).
const char* const kColorNames[] = {"white", "blue", "green", "red"};
static_assert(static_cast<int>(mq::Color::White) == 0 && static_cast<int>(mq::Color::Blue) == 1 &&
                  static_cast<int>(mq::Color::Green) == 2 && static_cast<int>(mq::Color::Red) == 3,
              "kColorNames must follow mq::Color order");

// Worst case: every character of the value is '\n'.
constexpr size_t kMaxLines = mq::kValueMaxLen + 1;

enum FontId { kBarFont, kTitleFont, kSmallFont, kLargeFont, kFontCount };

struct FontMetrics {
    const uint8_t* data;
    int ascent;      // top of the tallest glyph above the cursor line
    int capHeight;   // height of 'H', for vertical centering
    int lineHeight;  // distance between wrapped lines
    int xRight;      // right edge of "X", used to measure advances
};

struct Layout {
    int barTop;       // top of the pill row
    int barSide;      // left and right inset of the pill row
    int pillRadius;
    int countdownRadius;
    int countdownMargin;  // inset of the countdown and arrival boxes from the bottom corners
    int boxTop;           // top of the countdown and arrival boxes
    int arrivalWidth;     // widest text the arrival box shows without scrolling
    int titleLeft;        // title text area, clear of rounded corners; the rule under
    int titleWidth;       // the title spans the same width
    int ruleTop;
    int pillHeight;
    int barHeight;
    int titleTop;
    int valueTop;
    int valueHeight;       // value area without boxes: down to the bottom edge
    int boxedValueHeight;  // value area with boxes: ends above the box row
    int countdownHeight;
};

enum class Align { Left, Center, Right };

Arduino_GFX* display = nullptr;
Arduino_Canvas_Indexed* canvas = nullptr;
FontMetrics fonts[kFontCount];
Layout layoutRows;

// Layout of the message on screen; rebuilt only when its text, font, or color
// changes, so scrolling does not restart when other messages change.
struct Shown {
    bool valid;
    char title[mq::kTitleMaxLen + 1];
    char value[mq::kValueMaxLen + 1];      // as received, with markup
    char text[mq::kValueMaxLen + 1];       // visible text, markup removed
    uint8_t colors[mq::kValueMaxLen + 1];  // mq::Color per visible character
    mq::FontSize fontSize;
    mq::Color color;
    int titleWidth;
    int lineHeight;
    size_t lineCount;
    uint64_t sinceMs;
    int valueHeight;  // layoutRows.valueHeight, or boxedValueHeight while a box shows
    char arrival[24];
    int arrivalWidth;
    uint64_t arrivalSinceMs;
};

Shown shown;
ui::Line lines[kMaxLines];
char lineBuffer[mq::kValueMaxLen + 2];  // one extra for the measuring 'X'
uint64_t lastFrameMs = 0;
int32_t lastCountdownS = -1;  // -1: no countdown on screen
StatusBar lastBar;
bool lastBarValid = false;
uint64_t popupUntilMs = 0;
bool popupShown = false;
bool panelOn = true;

// Screen saver particle hues (F-014) at full brightness. Each is drawn in
// kParticleLevels - 1 fixed brightness steps, 42 colors in all: the indexed
// canvas's palette holds 255 colors for the whole run and loses color depth
// when it overflows, so the animation must reuse the same colors every frame.
struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};
constexpr Rgb kParticleHueRgb[ui::kParticleHues] = {
    {0, 220, 255},    // cyan
    {255, 60, 200},   // magenta
    {255, 170, 0},    // amber
    {80, 255, 120},   // green
    {90, 130, 255},   // blue
    {235, 235, 255},  // white
};

// QR code modules, copied out of the ESP-IDF encoder's callback.
constexpr int kQrMaxModules = 37;  // version 5; the provisioning payload needs version 4
bool qrModules[kQrMaxModules * kQrMaxModules];
int qrSize = 0;

uint16_t particleColor(uint8_t hue, uint8_t level)
{
    const Rgb& rgb = kParticleHueRgb[hue];
    const int top = kParticleLevels - 1;
    const int r = rgb.r * level / top;
    const int g = rgb.g * level / top;
    const int b = rgb.b * level / top;
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

int pixel(float coordinate)
{
    return static_cast<int>(coordinate + 0.5f);
}

int smaller(int a, int b)
{
    return a < b ? a : b;
}

// Right edge of the text's bounding box when drawn from x = 0.
int boundsRight(const char* text)
{
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    canvas->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    return x1 + w;
}

void measureFont(FontId id, const uint8_t* data)
{
    int16_t x1 = 0;
    int16_t y1 = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    FontMetrics& font = fonts[id];
    font.data = data;
    canvas->setFont(data);
    canvas->getTextBounds("H", 0, 0, &x1, &y1, &w, &h);
    font.capHeight = h;
    canvas->getTextBounds("Hbdfhkl|", 0, 0, &x1, &y1, &w, &h);
    font.ascent = -y1;
    canvas->getTextBounds("gjpqy|", 0, 0, &x1, &y1, &w, &h);
    const int descent = y1 + h;
    font.lineHeight = font.ascent + descent + (font.ascent + 7) / 8;
    font.xRight = boundsRight("X");
}

// Pen advance of text: bounds only cover inked pixels, so a trailing space
// would be lost. Appending 'X' and subtracting its own right edge gives the
// exact advance, spaces included.
int advance(const char* text, size_t length, FontId font)
{
    if (length == 0) {
        return 0;
    }
    memcpy(lineBuffer, text, length);
    lineBuffer[length] = 'X';
    lineBuffer[length + 1] = '\0';
    canvas->setFont(fonts[font].data);
    return boundsRight(lineBuffer) - fonts[font].xRight;
}

int textWidth(const char* text, FontId font)
{
    return advance(text, strlen(text), font);
}

// Draws one line of text with its top at top; returns the pen advance.
int drawText(const char* text, int x, int top, Align align, FontId font, uint16_t color)
{
    const int width = textWidth(text, font);
    const int left = align == Align::Center ? x - width / 2 : align == Align::Right ? x - width : x;
    canvas->setFont(fonts[font].data);
    canvas->setTextColor(color);
    canvas->setCursor(left, top + fonts[font].ascent);
    canvas->print(text);
    return width;
}

// Draws one line of text centered vertically on centerY (by its cap height).
int drawTextMiddle(const char* text, int x, int centerY, Align align, FontId font, uint16_t color)
{
    const FontMetrics& metrics = fonts[font];
    return drawText(text, x, centerY + metrics.capHeight / 2 - metrics.ascent, align, font, color);
}

// Horizontal inset for a shape with corner radius r whose top edge is at top,
// so that it stays inside a circular display corner of radius kCornerRadiusMax.
int cornerClearance(int top, int r)
{
    const float big = kCornerRadiusMax - r;
    const float dy = kCornerRadiusMax - top - r;
    if (dy <= 0) {
        return 0;
    }
    return static_cast<int>(big - sqrtf(big * big - dy * dy)) + 2;
}

// Inset from a corner along the diagonal for a shape with corner radius r.
int diagonalClearance(int r)
{
    return static_cast<int>((kCornerRadiusMax - r) * 0.2929f) + 2;
}

void computeLayout()
{
    Layout& rows = layoutRows;
    rows.pillHeight = fonts[kBarFont].lineHeight + 2;
    rows.pillRadius = kRoundedCorners ? rows.pillHeight / 2 : 4;
    rows.barTop = kRoundedCorners ? kRoundedBarTop : 0;
    rows.barSide = kRoundedCorners ? cornerClearance(rows.barTop + 1, rows.pillRadius) : 0;
    rows.barHeight = rows.barTop + rows.pillHeight + 2;
    // Title: white, with a grey rule under it; both clear of rounded corners.
    rows.titleTop = rows.barHeight + (kRoundedCorners ? 8 : 3);  // rounded boards: room below the band
    rows.titleLeft = kMargin;
    if (kRoundedCorners) {
        const int clearance = cornerClearance(rows.titleTop, 0);
        rows.titleLeft = clearance > kMargin ? clearance : kMargin;
    }
    rows.titleWidth = kWidth - 2 * rows.titleLeft;
    rows.ruleTop = rows.titleTop + fonts[kTitleFont].lineHeight + 2;
    rows.valueTop = rows.ruleTop + (kRoundedCorners ? 5 : 2);
    rows.valueHeight = kHeight - rows.valueTop;
    rows.countdownHeight = fonts[kSmallFont].lineHeight + 4;
    rows.countdownRadius = kRoundedCorners ? rows.countdownHeight / 2 : 3;
    rows.countdownMargin = kRoundedCorners ? diagonalClearance(rows.countdownRadius) : 0;
    rows.boxTop = kHeight - rows.countdownHeight - 2 - rows.countdownMargin;
    rows.boxedValueHeight = rows.boxTop - 2 - rows.valueTop;
    // As wide as the date, so a full "HH:MM DD-MM-YYYY" scrolls from the time to the date.
    rows.arrivalWidth = textWidth("88-88-8888", kSmallFont);
}

FontId fontFor(mq::FontSize size)
{
    return size == mq::FontSize::Large ? kLargeFont : kSmallFont;
}

uint16_t colorFor(mq::Color color)
{
    switch (color) {
    case mq::Color::Blue:
        return kBlue;
    case mq::Color::Green:
        return kColorGreen;
    case mq::Color::Red:
        return kColorRed;
    case mq::Color::White:
        break;
    }
    return kTextGrey;  // a bit darker than the white title
}

int measureText(const char* text, size_t length, void* context)
{
    return advance(text, length, *static_cast<FontId*>(context));
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
    FontId font = fontFor(message.fontSize);
    shown.titleWidth = textWidth(shown.title, kTitleFont);
    shown.lineHeight = fonts[font].lineHeight;
    shown.lineCount = ui::wrapText(shown.text, kTextWidth, measureText, &font, lines, kMaxLines);
    shown.sinceMs = nowMs;
    shown.valid = true;
}

// Keeps the arrival text; a new text restarts its scrolling.
void setArrival(const char* arrival, uint64_t nowMs)
{
    snprintf(shown.arrival, sizeof(shown.arrival), "%s", arrival);
    shown.arrivalWidth = textWidth(shown.arrival, kSmallFont);
    shown.arrivalSinceMs = nowMs;
}

bool isScrolling()
{
    return shown.valid && (shown.titleWidth > layoutRows.titleWidth ||
                           static_cast<int>(shown.lineCount) * shown.lineHeight > shown.valueHeight ||
                           shown.arrivalWidth > layoutRows.arrivalWidth);
}

// Draws one wrapped line as runs of equal color, left to right.
void drawLine(const ui::Line& line, int top, FontId font)
{
    int x = kMargin;
    size_t pos = line.start;
    const size_t end = line.start + line.length;
    char run[mq::kValueMaxLen + 1];
    while (pos < end) {
        const uint8_t color = shown.colors[pos];
        size_t runEnd = pos + 1;
        while (runEnd < end && shown.colors[runEnd] == color) {
            runEnd++;
        }
        memcpy(run, shown.text + pos, runEnd - pos);
        run[runEnd - pos] = '\0';
        x += drawText(run, x, top, Align::Left, font, colorFor(static_cast<mq::Color>(color)));
        pos = runEnd;
    }
}

void drawValue(const mq::Message& message, uint64_t elapsedMs)
{
    const FontId font = fontFor(message.fontSize);
    const int contentHeight = static_cast<int>(shown.lineCount) * shown.lineHeight;
    const int bottom = layoutRows.valueTop + shown.valueHeight;
    const int offset = ui::scrollOffset(contentHeight, shown.valueHeight, elapsedMs, kValueSpeedPxPerS, kScrollPauseMs);

    for (size_t i = 0; i < shown.lineCount; i++) {
        const int top = layoutRows.valueTop + static_cast<int>(i) * shown.lineHeight - offset;
        if (top + shown.lineHeight <= layoutRows.valueTop) {
            continue;
        }
        if (top >= bottom) {
            break;
        }
        drawLine(lines[i], top, font);
    }
    // With boxes the value area ends above them, as if the screen ended there:
    // nothing shows in the box row.
    if (bottom < kHeight) {
        canvas->fillRect(0, bottom, kWidth, kHeight - bottom, kBlack);
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
    const int height = layoutRows.countdownHeight;
    const int width = textWidth(text, kSmallFont) + 2 * kCountdownPadding;
    const int radius = layoutRows.countdownRadius;
    const int x = kWidth - width - 2 - layoutRows.countdownMargin;
    const int y = layoutRows.boxTop;
    canvas->fillRoundRect(x, y, width, height, radius, kBlack);
    canvas->drawRoundRect(x, y, width, height, radius, kDarkGrey);
    drawTextMiddle(text, x + width / 2, y + height / 2, Align::Center, kSmallFont, kColorLightGrey);
}

// Box at the bottom left with the arrival time (F-015), styled like the
// countdown. A full date is wider than the box and scrolls sideways inside it.
void drawArrival(uint64_t elapsedMs)
{
    const int height = layoutRows.countdownHeight;
    const int radius = layoutRows.countdownRadius;
    const int innerWidth = smaller(shown.arrivalWidth, layoutRows.arrivalWidth);
    const int width = innerWidth + 2 * kCountdownPadding;
    const int x = 2 + layoutRows.countdownMargin;
    const int y = layoutRows.boxTop;
    const int innerLeft = x + kCountdownPadding;
    const int innerRight = innerLeft + innerWidth;
    const int offset =
        ui::scrollOffset(shown.arrivalWidth, innerWidth, elapsedMs, kArrivalSpeedPxPerS, kScrollPauseMs);
    drawTextMiddle(shown.arrival, innerLeft - offset, y + height / 2, Align::Left, kSmallFont, kColorLightGrey);
    // Clip the text to the inside of the box; the box row is otherwise empty.
    canvas->fillRect(0, y, innerLeft, height, kBlack);
    canvas->fillRect(innerRight, y, kWidth - innerRight, height, kBlack);
    canvas->drawRoundRect(x, y, width, height, radius, kDarkGrey);
}

void drawEmpty()
{
    drawTextMiddle("No messages", kWidth / 2, layoutRows.valueTop + layoutRows.valueHeight / 2, Align::Center,
                   kSmallFont, kDarkGrey);
}

// Rounded background for one top bar element.
void drawPill(int x, int width, uint16_t color)
{
    canvas->fillRoundRect(x, layoutRows.barTop + 1, width, layoutRows.pillHeight, layoutRows.pillRadius, color);
}

// Battery outline, sized to the pill (18 x 9 px on the T-Display). On battery:
// a fill level, green above 50 %, amber above 20 %, red below. On USB power: an
// amber lightning bolt. Unknown: empty.
int batteryIconWidth()
{
    return (layoutRows.pillHeight / 2) * 16 / 9 + 2;
}

void drawBatteryIcon(int x, const StatusBar& bar)
{
    const int bodyHeight = layoutRows.pillHeight / 2;
    const int bodyWidth = bodyHeight * 16 / 9;
    const int y = layoutRows.barTop + 1 + (layoutRows.pillHeight - bodyHeight) / 2;
    const int nub = bodyHeight / 3;
    canvas->drawRect(x, y, bodyWidth, bodyHeight, kColorLightGrey);
    canvas->fillRect(x + bodyWidth, y + nub, 2, bodyHeight - 2 * nub, kColorLightGrey);
    if (!bar.batteryKnown) {
        return;
    }
    if (bar.externalPower) {
        const int cx = x + bodyWidth / 2;
        const int mid = y + bodyHeight / 2;
        const int step = bodyHeight / 4;
        canvas->drawLine(cx + step / 2 + 1, y + 1, cx - step / 2 - 1, mid, kQueueAccent);
        canvas->drawLine(cx - step / 2 - 1, mid, cx + step / 2 + 1, mid, kQueueAccent);
        canvas->drawLine(cx + step / 2 + 1, mid, cx - step / 2 - 1, y + bodyHeight - 2, kQueueAccent);
        return;
    }
    const uint16_t fill = bar.batteryPercent > 50 ? kColorGreen : bar.batteryPercent > 20 ? kQueueAccent : kColorRed;
    const int fillWidth = (bodyWidth - 4) * bar.batteryPercent / 100;
    if (fillWidth > 0) {
        canvas->fillRect(x + 2, y + 2, fillWidth, bodyHeight - 4, fill);
    }
}

uint16_t linkColor(Link link)
{
    switch (link) {
    case Link::Up:
        return kColorGreen;
    case Link::Pending:
        return kQueueAccent;
    case Link::Down:
        break;
    }
    return kColorRed;
}

// Dark bar with separate pills, laid out right to left: clock, battery icon,
// queue position (amber, bold), then the network pill fills the space on the
// left, with a status stripe on its left edge (D-028).
void drawTopBar(const mq::MessageQueue& queue, const StatusBar& bar)
{
    const int centerY = layoutRows.barTop + 1 + layoutRows.pillHeight / 2;
    if (kRoundedCorners) {
        // Neutral band from the top edge; the panel's rounded corners trim it.
        canvas->fillRect(0, 0, kWidth, layoutRows.barHeight + 2, kBandBackground);
        canvas->drawFastHLine(0, layoutRows.barHeight + 2, kWidth, kBandEdge);
    } else {
        canvas->fillRect(0, 0, kWidth, layoutRows.barHeight, kBarBackground);
    }

    // Clock
    const int clockWidth = textWidth("88:88", kBarFont) + 2 * kPillPadding;
    const int clockX = kWidth - 2 - layoutRows.barSide - clockWidth;
    drawPill(clockX, clockWidth, kPillBackground);
    drawTextMiddle(bar.clock, clockX + clockWidth / 2, centerY, Align::Center, kBarFont, kWhite);

    // Battery, only on boards with battery sense
    int batteryX = clockX;
    if (bar.hasBattery) {
        const int batteryWidth = batteryIconWidth() + 2 * kPillPadding;
        batteryX = clockX - kPillGap - batteryWidth;
        drawPill(batteryX, batteryWidth, kPillBackground);
        drawBatteryIcon(batteryX + kPillPadding, bar);
    }

    // Queue position: amber pill, black text drawn twice one pixel apart for a bold look
    char position[12];
    const size_t current = queue.empty() ? 0 : queue.cursor() + 1;
    snprintf(position, sizeof(position), "%u/%u", static_cast<unsigned>(current), static_cast<unsigned>(queue.size()));
    const int queueWidth = textWidth(position, kBarFont) + 1 + 2 * kPillPadding;
    const int queueX = batteryX - kPillGap - queueWidth;
    drawPill(queueX, queueWidth, kQueueAccent);
    drawTextMiddle(position, queueX + queueWidth / 2, centerY, Align::Center, kBarFont, kBlack);
    drawTextMiddle(position, queueX + queueWidth / 2 + 1, centerY, Align::Center, kBarFont, kBlack);

    // Network: status stripe, then the label in the remaining space
    constexpr int kStripeWidth = 3;
    const int networkX = 2 + layoutRows.barSide;
    const int networkWidth = queueX - kPillGap - networkX;
    drawPill(networkX, networkWidth, kPillBackground);
    int textX = networkX + 2 + kStripeWidth + 3;
    if (kRoundedCorners) {
        // A stripe would stick out of the capsule's rounded end; use a dot inside it.
        const int r = layoutRows.pillRadius;
        canvas->fillCircle(networkX + r, centerY, r / 2, linkColor(bar.link));
        textX = networkX + 2 * r;
    } else {
        canvas->fillRect(networkX + 2, 4, kStripeWidth, layoutRows.pillHeight - 6, linkColor(bar.link));
    }
    drawTextMiddle(bar.network, textX, centerY, Align::Left, kBarFont, kColorLightGrey);
}

void drawTitle(uint64_t elapsedMs)
{
    const Layout& rows = layoutRows;
    const int offset = ui::scrollOffset(shown.titleWidth, rows.titleWidth, elapsedMs, kTitleSpeedPxPerS, kScrollPauseMs);
    drawText(shown.title, rows.titleLeft - offset, rows.titleTop, Align::Left, kTitleFont, kWhite);
    // Clip a scrolling title to the rule's width.
    const int right = rows.titleLeft + rows.titleWidth;
    canvas->fillRect(0, rows.titleTop, rows.titleLeft, rows.ruleTop - rows.titleTop, kBlack);
    canvas->fillRect(right, rows.titleTop, kWidth - right, rows.ruleTop - rows.titleTop, kBlack);
    canvas->drawFastHLine(rows.titleLeft, rows.ruleTop, rows.titleWidth, kTitleRule);
}

void drawQueueFullPopup()
{
    const char* kLine1 = "Queue full";
    const char* kLine2 = "new messages dropped";
    const int lineHeight = fonts[kSmallFont].lineHeight;
    const int width = textWidth(kLine2, kSmallFont) + 24;
    const int height = 2 * lineHeight + 14;
    const int x = (kWidth - width) / 2;
    const int y = (kHeight - height) / 2;
    canvas->fillRoundRect(x, y, width, height, 5, kBlack);
    canvas->drawRoundRect(x, y, width, height, 5, kColorRed);
    canvas->drawRoundRect(x + 1, y + 1, width - 2, height - 2, 4, kColorRed);
    drawText(kLine1, kWidth / 2, y + 7, Align::Center, kSmallFont, kColorRed);
    drawText(kLine2, kWidth / 2, y + 7 + lineHeight, Align::Center, kSmallFont, kColorLightGrey);
}

void render(const mq::MessageQueue& queue, const mq::Message* message, int32_t countdownS, uint64_t nowMs,
            const StatusBar& bar, bool popup)
{
    canvas->fillScreen(kBlack);
    if (message != nullptr) {
        drawValue(*message, nowMs - shown.sinceMs);
        if (shown.arrival[0] != '\0') {
            drawArrival(nowMs - shown.arrivalSinceMs);
        }
        if (countdownS >= 0) {
            drawCountdown(static_cast<uint32_t>(countdownS));
        }
    } else {
        drawEmpty();
    }
    // The value scrolls under the header, so clear the header area before drawing it.
    canvas->fillRect(0, 0, kWidth, layoutRows.valueTop, kBlack);
    drawTopBar(queue, bar);
    if (message != nullptr) {
        drawTitle(nowMs - shown.sinceMs);
    }
    if (popup) {
        drawQueueFullPopup();
    }
    canvas->flush();
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
    canvas->fillRect(x0, y0, total, total, kWhite);
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (qrModules[y * qrSize + x]) {
                canvas->fillRect(x0 + (x + kQuiet) * scale, y0 + (y + kQuiet) * scale, scale, scale, kBlack);
            }
        }
    }
    return total;
}

// Draws lines top to bottom starting at top; gapBefore adds space above a line.
struct TextLine {
    const char* text;
    FontId font;
    uint16_t color;
    int gapBefore;
};

int blockHeight(const TextLine* textLines, size_t count)
{
    int height = 0;
    for (size_t i = 0; i < count; i++) {
        height += textLines[i].gapBefore + fonts[textLines[i].font].lineHeight;
    }
    return height;
}

void drawBlock(const TextLine* textLines, size_t count, int x, int top, Align align)
{
    int y = top;
    for (size_t i = 0; i < count; i++) {
        y += textLines[i].gapBefore;
        drawText(textLines[i].text, x, y, align, textLines[i].font, textLines[i].color);
        y += fonts[textLines[i].font].lineHeight;
    }
}

}  // namespace

bool begin()
{
    display = hal::display();
    // 8-bit indexed canvas: one byte per pixel plus a palette of exact RGB565
    // colors (32,400 bytes at 240 x 135; in PSRAM on boards that have it). The
    // panel's rotation matches the canvas.
    canvas = new Arduino_Canvas_Indexed(kWidth, kHeight, display, 0, 0, 0, 0);
    if (!canvas->begin(hal::displaySpeedHz())) {
        return false;
    }
    canvas->setTextWrap(false);
    const hal::Fonts boardFonts = hal::fonts();
    measureFont(kBarFont, boardFonts.bar);
    measureFont(kTitleFont, boardFonts.title);
    measureFont(kSmallFont, boardFonts.small);
    measureFont(kLargeFont, boardFonts.large);
    computeLayout();
    canvas->fillScreen(kBlack);
    canvas->flush();
    return true;
}

void showBootScreen()
{
    const TextLine textLines[] = {
        {"McpVue", kLargeFont, kWhite, 0},
        {"v" FW_VERSION, kSmallFont, kColorLightGrey, 6},
    };
    canvas->fillScreen(kBlack);
    drawBlock(textLines, 2, kWidth / 2, (kHeight - blockHeight(textLines, 2)) / 2, Align::Center);
    canvas->flush();
}

void update(const mq::MessageQueue& queue, uint64_t nowMs, bool queueChanged, const StatusBar& bar,
            const char* arrival)
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
    const bool arrivalChanged = strcmp(arrival, shown.arrival) != 0;
    if (relayout || arrivalChanged) {
        setArrival(arrival, nowMs);
    }

    const int32_t countdownS = countdownFor(message);
    const bool boxes = countdownS >= 0 || shown.arrival[0] != '\0';
    shown.valueHeight = boxes ? layoutRows.boxedValueHeight : layoutRows.valueHeight;
    const bool frameDue = isScrolling() && nowMs - lastFrameMs >= kFrameMs;
    const bool barChanged = !lastBarValid || strcmp(bar.network, lastBar.network) != 0 || bar.link != lastBar.link ||
                            bar.hasBattery != lastBar.hasBattery || strcmp(bar.clock, lastBar.clock) != 0 ||
                            bar.batteryKnown != lastBar.batteryKnown || bar.externalPower != lastBar.externalPower ||
                            bar.batteryPercent != lastBar.batteryPercent;
    const bool popup = nowMs < popupUntilMs;
    if (!queueChanged && !relayout && !arrivalChanged && !frameDue && countdownS == lastCountdownS && !barChanged &&
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

    char code[24];
    snprintf(code, sizeof(code), "Code %s", pop);
    const int gap = fonts[kSmallFont].lineHeight / 3;
    const TextLine textLines[] = {
        {"Wi-Fi setup", kSmallFont, kWhite, 0},
        {"ESP BLE", kSmallFont, kColorLightGrey, gap},
        {"Provisioning app", kSmallFont, kColorLightGrey, 0},
        {serviceName, kSmallFont, kWhite, gap},
        {code, kSmallFont, kWhite, 0},
        {status, kSmallFont, statusColor, gap},
    };
    constexpr size_t kLines = sizeof(textLines) / sizeof(textLines[0]);
    const int textHeight = blockHeight(textLines, kLines);

    canvas->fillScreen(kBlack);
    const bool portrait = kHeight > kWidth;
    if (portrait) {
        // QR code on top, text centered below.
        int qrTotal = 0;
        if (qrSize > 0) {
            const int scale = smaller(8, smaller(kWidth - 2 * kMargin, kHeight - textHeight - 3 * kMargin) / (qrSize + 4));
            qrTotal = (qrSize + 4) * scale;
        }
        const int top = (kHeight - qrTotal - kMargin - textHeight) / 2;
        if (qrSize > 0) {
            drawQr((kWidth - qrTotal) / 2, top, qrTotal / (qrSize + 4));
        }
        drawBlock(textLines, kLines, kWidth / 2, top + qrTotal + kMargin, Align::Center);
    } else {
        // QR code on the left, text to its right; the largest whole-pixel scale that fits the height.
        int textX = kMargin;
        if (qrSize > 0) {
            const int scale = smaller(4, (kHeight - 4) / (qrSize + 4));
            const int size = (qrSize + 4) * scale;
            textX += drawQr(kMargin, (kHeight - size) / 2, scale) + 6;
        }
        drawBlock(textLines, kLines, textX, (kHeight - textHeight) / 2, Align::Left);
    }
    canvas->flush();
}

void showWelcome(const char* ssid, const char* ip)
{
    char connected[48];
    snprintf(connected, sizeof(connected), "Connected to %s", ssid);
    char url[48];
    snprintf(url, sizeof(url), "%s.local/mcp", board::kHostname);
    const int gap = fonts[kSmallFont].lineHeight / 2;
    const TextLine textLines[] = {
        {"McpVue v" FW_VERSION, kSmallFont, kColorLightGrey, 0},
        {connected, kSmallFont, kWhite, gap},
        {ip, kLargeFont, kColorGreen, gap},
        {url, kSmallFont, kColorLightGrey, gap},
    };
    constexpr size_t kLines = sizeof(textLines) / sizeof(textLines[0]);
    canvas->fillScreen(kBlack);
    drawBlock(textLines, kLines, kWidth / 2, (kHeight - blockHeight(textLines, kLines)) / 2, Align::Center);
    canvas->flush();
}

void showCornerTest()
{
    // Colored 8 x 8 px squares along each corner's diagonal, moved in by 0 to
    // 48 px, and a 1 px white frame on the screen edges. The first square that
    // is complete in every corner gives the corner margin; a missing edge line
    // means a display offset rather than the corner radius.
    struct Mark {
        int inset;
        uint16_t color;
        const char* name;
    };
    const Mark marks[] = {
        {0, kWhite, "0 white"},    {8, kColorGreen, "8 green"},      {16, kBlue, "16 blue"},
        {24, kColorRed, "24 red"}, {32, kQueueAccent, "32 amber"}, {40, kColorLightGrey, "40 grey"},
        {48, 0xF81F, "48 pink"},
    };
    constexpr int kSquare = 8;
    canvas->fillScreen(kBlack);
    canvas->drawRect(0, 0, kWidth, kHeight, kWhite);
    for (const Mark& mark : marks) {
        const int near = mark.inset;
        const int far = kWidth - kSquare - mark.inset;
        const int bottom = kHeight - kSquare - mark.inset;
        canvas->fillRect(near, near, kSquare, kSquare, mark.color);
        canvas->fillRect(far, near, kSquare, kSquare, mark.color);
        canvas->fillRect(far, bottom, kSquare, kSquare, mark.color);
        canvas->fillRect(near, bottom, kSquare, kSquare, mark.color);
    }
    const int lineHeight = fonts[kSmallFont].lineHeight;
    int top = (kHeight - 10 * lineHeight) / 2;
    drawText("Corner test", kWidth / 2, top, Align::Center, kSmallFont, kWhite);
    top += lineHeight;
    drawText("first square complete", kWidth / 2, top, Align::Center, kSmallFont, kColorLightGrey);
    top += lineHeight;
    drawText("in all 4 corners:", kWidth / 2, top, Align::Center, kSmallFont, kColorLightGrey);
    for (const Mark& mark : marks) {
        top += lineHeight;
        drawText(mark.name, kWidth / 2, top, Align::Center, kSmallFont, mark.color);
    }
    canvas->flush();
}

void showQueueFullPopup(uint64_t nowMs)
{
    popupUntilMs = nowMs + kPopupMs;
}

void sendScreenshot(Print& out)
{
    const uint8_t* pixels = canvas != nullptr ? canvas->getFramebuffer() : nullptr;
    if (pixels == nullptr) {
        out.println("SCREENSHOT unavailable");
        return;
    }
    static const char kHex[] = "0123456789abcdef";
    static char row[kWidth * 2 + 1];
    out.printf("SCREENSHOT %d %d indexed565\n", kWidth, kHeight);
    // Palette line: 256 RGB565 colors, 4 hex digits each.
    const uint16_t* palette = canvas->getColorIndex();
    for (int i = 0; i < 256; i++) {
        out.printf("%04x", palette[i]);
    }
    out.println();
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
    canvas->fillScreen(kBlack);
    drawTextMiddle(text, kWidth / 2, kHeight / 2, Align::Center, kSmallFont, kWhite);
    canvas->flush();
}

void sleep()
{
    if (!panelOn) {
        return;
    }
    // The panel keeps its memory while asleep; black there means waking it
    // never shows an old message for a moment.
    canvas->fillScreen(kBlack);
    canvas->flush();
    hal::setDisplayOn(false);
    panelOn = false;
}

void wake()
{
    if (panelOn) {
        return;
    }
    hal::setDisplayOn(true);
    panelOn = true;
    // A screen drawn while the panel was off (for example the corner test)
    // reaches it now; the message screen is redrawn in full by update().
    canvas->flush();
    lastBarValid = false;
}

void drawParticles(const ui::ParticleField& field, uint8_t level, const SaverText& text)
{
    // Sizes from the shorter screen side: 1 px trails and a 3 px head at 135 px,
    // 2 px trails and a 7 px head on the AMOLED's 368 px.
    constexpr int kSide = kWidth < kHeight ? kWidth : kHeight;
    constexpr int kHeadRadius = kSide / 120;
    constexpr bool kWideTrail = kSide >= 300;
    constexpr int kTop = kParticleLevels - 1;
    // Glow blobs: up to 0.22 of the shorter side (30 px at 135 px, 81 px at 368 px),
    // all in blue at the lowest brightness step, merging into one dim glow.
    constexpr int kGlowRadius = kSide * 22 / 100;
    constexpr uint8_t kGlowHue = 4;     // blue in kParticleHueRgb
    constexpr uint8_t kWhiteHue = 5;
    const uint8_t glow = text.glow < level ? text.glow : level;
    const uint8_t trailLevel = static_cast<uint8_t>(level * (kTop - glow) / kTop);
    canvas->fillScreen(kBlack);
    if (glow > 0) {
        const int radius = kGlowRadius * glow / kTop;
        const uint16_t color = particleColor(kGlowHue, 1);
        for (int i = 0; i < field.count(); ++i) {
            const ui::Particle& p = field.particle(i);
            canvas->fillCircle(pixel(p.trail[0].x), pixel(p.trail[0].y), radius, color);
        }
    }
    for (int i = 0; trailLevel > 0 && i < field.count(); ++i) {
        const ui::Particle& p = field.particle(i);
        // Oldest segment first; each is dimmer the older it is.
        for (int j = p.trailCount - 1; j > 0; --j) {
            const uint8_t segment = static_cast<uint8_t>(trailLevel * (ui::kTrailLength - j + 1) / ui::kTrailLength);
            if (segment == 0) {
                continue;
            }
            const uint16_t color = particleColor(p.hue, segment);
            const int x0 = pixel(p.trail[j].x);
            const int y0 = pixel(p.trail[j].y);
            const int x1 = pixel(p.trail[j - 1].x);
            const int y1 = pixel(p.trail[j - 1].y);
            canvas->drawLine(x0, y0, x1, y1, color);
            if (kWideTrail) {
                canvas->drawLine(x0 + 1, y0, x1 + 1, y1, color);
                canvas->drawLine(x0, y0 + 1, x1, y1 + 1, color);
            }
        }
        canvas->fillCircle(pixel(p.trail[0].x), pixel(p.trail[0].y), kHeadRadius, particleColor(p.hue, trailLevel));
    }
    if (text.level > 0) {
        constexpr int kGap = 2;
        const int clockHeight = fonts[kLargeFont].lineHeight;
        const int top = (kHeight - clockHeight - kGap - fonts[kSmallFont].lineHeight) / 2;
        const uint8_t dateLevel = text.level > 2 ? text.level - 2 : text.level;
        drawText(text.clock, kWidth / 2, top, Align::Center, kLargeFont, particleColor(kWhiteHue, text.level));
        drawText(text.date, kWidth / 2, top + clockHeight + kGap, Align::Center, kSmallFont,
                 particleColor(kWhiteHue, dateLevel));
    }
    canvas->flush();
    lastBarValid = false;
}

}  // namespace screen
