#include <Arduino.h>
#include <WiFi.h>
#include <battery_level.h>
#include <button_pair.h>
#include <esp_timer.h>
#include <message_queue.h>

#include "battery.h"
#include "clock.h"
#include "mcp_server.h"
#include "network.h"
#include "pins.h"
#include "screen.h"
#include "storage.h"

namespace {

constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kLongPressMs = 1500;  // hold delete this long to clear all
constexpr uint32_t kBootScreenMs = 1500;
constexpr uint32_t kSaveDelayMs = 1000;  // one flash write for a burst of changes
constexpr uint32_t kWifiResetHoldMs = 5000;  // hold both buttons this long to forget Wi-Fi
constexpr uint32_t kWelcomeMs = 3000;
constexpr uint32_t kIpMessageS = 60;  // on-screen time of the IP message (D-027)
constexpr uint32_t kBatteryReadMs = 10000;

mq::MessageQueue queue;
ui::ButtonPair buttons(kDebounceMs, kLongPressMs, kWifiResetHoldMs);  // first: delete, second: scroll
bool saveDue = false;
uint64_t saveAtMs = 0;
// True while the setup or welcome screen covers the messages, so the message
// screen is redrawn when it returns.
bool coverShown = false;
bool setupShown = false;
network::SetupStatus shownSetupStatus = network::SetupStatus::Waiting;
network::State lastState = network::State::Connecting;
bool welcomeDone = false;
uint64_t welcomeUntilMs = 0;
char announcedIp[16] = "";
ui::BatteryLevel batteryLevel{false, 0};
bool batteryKnown = false;
uint64_t lastBatteryReadMs = 0;

// 64-bit uptime; millis() is 32-bit and wraps after about 49.7 days.
uint64_t nowMs()
{
    return static_cast<uint64_t>(esp_timer_get_time()) / 1000;
}

// Returns true if the screen needs a redraw. Sets contentChanged when messages
// were removed; scrolling alone is not saved, so it does not set it.
bool handleButton(ui::PairEvent event, bool& contentChanged)
{
    switch (event) {
    case ui::PairEvent::FirstShort:
        if (!queue.deleteCurrent()) {
            return false;
        }
        contentChanged = true;
        Serial.printf("Deleted message, %u left\n", static_cast<unsigned>(queue.size()));
        return true;
    case ui::PairEvent::FirstLong:
        if (queue.empty()) {
            return false;
        }
        queue.clear();
        contentChanged = true;
        Serial.println("Cleared all messages");
        return true;
    case ui::PairEvent::SecondShort:
        if (queue.size() < 2) {
            return false;
        }
        queue.scrollNext();
        Serial.printf("Showing message %u of %u\n", static_cast<unsigned>(queue.cursor() + 1),
                      static_cast<unsigned>(queue.size()));
        return true;
    case ui::PairEvent::BothLong:
        screen::showNotice("Wi-Fi reset, restarting");
        network::resetAndRestart();
        return false;
    case ui::PairEvent::SecondLong:  // no action
    case ui::PairEvent::None:
        break;
    }
    return false;
}

const char* setupStatusText(network::SetupStatus status)
{
    switch (status) {
    case network::SetupStatus::Connecting:
        return "Connecting...";
    case network::SetupStatus::WrongPassword:
        return "Wrong password";
    case network::SetupStatus::NotFound:
        return "Network not found";
    case network::SetupStatus::Waiting:
        break;
    }
    return "Waiting for app";
}

uint16_t setupStatusColor(network::SetupStatus status)
{
    switch (status) {
    case network::SetupStatus::Connecting:
        return TFT_GREEN;
    case network::SetupStatus::WrongPassword:
    case network::SetupStatus::NotFound:
        return TFT_RED;
    case network::SetupStatus::Waiting:
        break;
    }
    return TFT_LIGHTGREY;
}

// Setup screen while provisioning runs (D-023). Messages are not on screen, so
// their countdown is paused.
void runSetupScreen()
{
    const network::SetupStatus status = network::setupStatus();
    if (!setupShown || status != shownSetupStatus) {
        screen::showSetup(network::qrPayload(), network::serviceName(), network::pop(), setupStatusText(status),
                          setupStatusColor(status));
        setupShown = true;
        coverShown = true;
        shownSetupStatus = status;
    }
    queue.pauseCountdown();
}

// Adds or replaces the IP message (F-009). Returns true if the queue changed.
bool addIpMessage(const char* ip)
{
    char value[160];
    snprintf(value, sizeof(value), "{green}Connected{/} to %s\nIP %s\nMCP http://screenapi.local/mcp",
             WiFi.SSID().c_str(), ip);
    const mq::NewMessage message{"ip", "Network", value, mq::FontSize::Small, mq::Color::White, mq::Kind::Timed,
                                 kIpMessageS};
    const mq::AddResult result = queue.add(message);
    if (result == mq::AddResult::Full) {
        Serial.println("IP message dropped: queue full");
    }
    return result == mq::AddResult::Added || result == mq::AddResult::Replaced;
}

// On connecting: the welcome screen once per boot, and the IP message when the
// address is new for this boot (F-009, D-027). Returns true if the queue changed.
bool announceConnection(uint64_t now)
{
    const network::State state = network::state();
    if (state == lastState) {
        return false;
    }
    lastState = state;
    if (state != network::State::Connected) {
        return false;
    }
    Serial.printf("Free heap after Wi-Fi connect: %u bytes, largest block: %u bytes\n",
                  static_cast<unsigned>(ESP.getFreeHeap()), static_cast<unsigned>(ESP.getMaxAllocHeap()));

    char ip[16];
    snprintf(ip, sizeof(ip), "%s", WiFi.localIP().toString().c_str());
    if (!welcomeDone) {
        welcomeDone = true;
        welcomeUntilMs = now + kWelcomeMs;
        coverShown = true;
        screen::showWelcome(WiFi.SSID().c_str(), ip);
    }
    if (strcmp(ip, announcedIp) == 0) {
        return false;
    }
    snprintf(announcedIp, sizeof(announcedIp), "%s", ip);
    return addIpMessage(ip);
}

// Serial commands for development (F-012): 'S' sends a screenshot.
void handleSerialCommands()
{
    while (Serial.available() > 0) {
        const int command = Serial.read();
        if (command == 'S') {
            screen::sendScreenshot(Serial);
        } else if (command == 'B') {
            Serial.printf("Battery: %u mV\n", static_cast<unsigned>(battery::readMillivolts()));
        }
    }
}

void readBatteryIfDue(uint64_t now)
{
    if (batteryKnown && now - lastBatteryReadMs < kBatteryReadMs) {
        return;
    }
    lastBatteryReadMs = now;
    batteryLevel = ui::batteryLevel(battery::readMillivolts());
    batteryKnown = true;
}

screen::StatusBar statusBar()
{
    screen::StatusBar bar;
    network::label(bar.network, sizeof(bar.network));
    switch (network::state()) {
    case network::State::Connected:
        bar.link = screen::Link::Up;
        break;
    case network::State::Connecting:
    case network::State::Setup:
        bar.link = screen::Link::Pending;
        break;
    case network::State::Offline:
        bar.link = screen::Link::Down;
        break;
    }
    clock_sync::text(bar.clock, sizeof(bar.clock));
    bar.batteryKnown = batteryKnown;
    bar.externalPower = batteryLevel.external;
    bar.batteryPercent = batteryLevel.percent;
    return bar;
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
    battery::begin();

    if (!screen::begin()) {
        Serial.println("Screen buffer allocation failed");
    }
    screen::showBootScreen();
    // Start Wi-Fi during the boot screen, so the setup event has arrived before the first frame.
    network::begin();
    mcp_server::begin(queue);
    delay(kBootScreenMs);

    storage::begin();
    const size_t restored = storage::load(queue);
    Serial.printf("Restored %u messages\n", static_cast<unsigned>(restored));
    Serial.printf("Free heap: %u bytes, largest block: %u bytes\n", static_cast<unsigned>(ESP.getFreeHeap()),
                  static_cast<unsigned>(ESP.getMaxAllocHeap()));
}

void loop()
{
    const uint64_t now = nowMs();
    network::poll(now);
    handleSerialCommands();
    // Buttons are read in every mode so their state stays consistent; events
    // are ignored while the setup or welcome screen is shown.
    const ui::PairEvent event =
        buttons.update(digitalRead(PIN_BUTTON_DELETE) == LOW, digitalRead(PIN_BUTTON_SCROLL) == LOW, now);

    if (network::state() == network::State::Setup) {
        runSetupScreen();
        saveIfDue(now);
        delay(20);
        return;
    }
    setupShown = false;

    bool contentChanged = announceConnection(now);
    const bool covered = now < welcomeUntilMs;
    bool redraw = contentChanged;
    if (covered) {
        queue.pauseCountdown();
    } else {
        // Tick before handling buttons, so time already spent is charged to the message that was on screen.
        if (queue.tick(now)) {
            contentChanged = true;
            Serial.printf("Timed message expired, %u left\n", static_cast<unsigned>(queue.size()));
        }
        redraw |= handleButton(event, contentChanged) || contentChanged;
    }

    const bool connected = network::state() == network::State::Connected;
    clock_sync::poll(now, connected);
    const mcp_server::Events mcpEvents = mcp_server::poll(connected);
    if (mcpEvents.messageDropped) {
        screen::showQueueFullPopup(now);
    }
    if (mcpEvents.queueChanged) {
        contentChanged = true;
        redraw = true;
    }
    if (contentChanged) {
        scheduleSave(now);
    }
    saveIfDue(now);

    if (!covered) {
        redraw |= coverShown;
        coverShown = false;
        readBatteryIfDue(now);
        screen::update(queue, now, redraw, statusBar());
    }
    delay(5);
}
