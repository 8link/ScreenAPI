#include <Arduino.h>
#include <WiFi.h>
#include <button_pair.h>
#include <esp_timer.h>
#include <message_queue.h>
#include <particles.h>
#include <screen_saver.h>
#include <soc/rtc.h>

#include "clock.h"
#include "mcp_server.h"
#include "network.h"
#include "board.h"
#include "hal.h"
#include "screen.h"
#include "storage.h"

namespace {

constexpr uint32_t kDebounceMs = 30;
constexpr uint32_t kLongPressMs = 1500;  // hold delete this long to clear all
constexpr uint32_t kBootScreenMs = 1500;
constexpr uint32_t kSaveDelayMs = 1000;  // one flash write for a burst of changes
constexpr uint32_t kWifiResetHoldMs = 5000;  // hold both buttons this long to forget Wi-Fi
constexpr uint32_t kWelcomeMs = 3000;
constexpr uint32_t kCornerTestMs = 60000;
constexpr uint32_t kIpMessageS = 60;  // on-screen time of the IP message (D-027)
constexpr uint32_t kBatteryReadMs = 10000;
// Screen saver (F-014): the panel goes off after the queue has been empty this
// long, and every period it shows a short particle animation.
constexpr uint32_t kSaverIdleMs = 60000;
constexpr uint32_t kSaverPeriodMs = 30000;
constexpr uint32_t kSaverAnimationMs = 10000;
constexpr uint32_t kSaverFadeMs = 600;
constexpr uint32_t kSaverFrameMs = 40;
// Clock in the animation, times from its start: the particles gather at the
// center, turn into a dim glow as the clock and date fade in, hold, and burst
// outward as the text fades out.
constexpr uint32_t kSaverGatherMs = 800;
constexpr uint32_t kSaverTextMs = 1600;
constexpr uint32_t kSaverTextLengthMs = 7000;  // fade in, hold 6.2 s, fade out
constexpr uint32_t kSaverTextRampMs = 400;
constexpr float kSaverGlowPace = 0.3f;  // the glow circles at 0.3 of the particles' speed
constexpr uint32_t kSaverBurstMs = kSaverTextMs + kSaverTextLengthMs - kSaverTextRampMs;
constexpr float kSaverBurstBoost = 3.0f;
static_assert(kSaverTextMs + kSaverTextLengthMs <= kSaverAnimationMs - kSaverFadeMs,
              "the clock must be gone before the animation fades out");
// CPU clock (F-014): 80 MHz is the lowest at which Wi-Fi runs. The animation
// runs at the top clock: the AMOLED's full-frame flush converts every pixel on
// the CPU.
constexpr uint32_t kCpuMhz = 160;
constexpr uint32_t kSaverCpuMhz = 80;
constexpr uint32_t kSaverAnimationCpuMhz = 240;

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
uint64_t coverUntilMs = 0;  // the welcome or corner test screen covers the messages until then
char announcedIp[16] = "";
hal::Battery battery{false, 0, 0};
bool batteryKnown = false;
uint64_t lastBatteryReadMs = 0;
ui::ScreenSaver saver(kSaverIdleMs, kSaverPeriodMs, kSaverAnimationMs);
ui::SaverPhase saverPhase = ui::SaverPhase::Awake;
ui::ParticleField particles;
bool swallowPress = false;  // a press that woke the screen does nothing else
uint64_t lastParticleFrameMs = 0;
// Clock and date of the current animation; empty while the time is not known.
char saverClock[8] = "";
char saverDate[12] = "";
bool saverGathering = false;
bool saverBurst = false;
bool saverSlowed = false;
uint32_t particleFrames = 0;
uint64_t particleRenderUs = 0;

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
        return screen::kColorGreen;
    case network::SetupStatus::WrongPassword:
    case network::SetupStatus::NotFound:
        return screen::kColorRed;
    case network::SetupStatus::Waiting:
        break;
    }
    return screen::kColorLightGrey;
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
    snprintf(value, sizeof(value), "{green}Connected{/} to %s\nIP %s\nMCP http://%s.local/mcp",
             WiFi.SSID().c_str(), ip, board::kHostname);
    const mq::NewMessage message{"ip", "Network", value, mq::FontSize::Small, mq::Color::White,
                                 mq::Kind::Timed, kIpMessageS, clock_sync::utcNow()};
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
        coverUntilMs = now + kWelcomeMs;
        coverShown = true;
        screen::showWelcome(WiFi.SSID().c_str(), ip);
    }
    if (strcmp(ip, announcedIp) == 0) {
        return false;
    }
    snprintf(announcedIp, sizeof(announcedIp), "%s", ip);
    return addIpMessage(ip);
}

// Serial commands for development (F-012): 'S' sends a screenshot, 'B' reads
// the battery, 'C' shows the corner test for 60 s.
void handleSerialCommands(uint64_t now)
{
    while (Serial.available() > 0) {
        const int command = Serial.read();
        if (command == 'S') {
            hal::setSerialBlocking(true);
            screen::sendScreenshot(Serial);
            hal::setSerialBlocking(false);
        } else if (command == 'C') {
            screen::showCornerTest();
            coverUntilMs = now + kCornerTestMs;
            coverShown = true;
            Serial.println("Corner test shown for 60 s");
        } else if (command == 'B') {
            if (hal::hasBattery()) {
                const hal::Battery reading = hal::readBattery();
                Serial.printf("Battery: %u mV, %d %%, %s\n", static_cast<unsigned>(reading.millivolts), reading.percent,
                              reading.external ? "USB power" : "on battery");
            } else {
                Serial.println("Battery: no battery sense on this board");
            }
        }
    }
}

void readBatteryIfDue(uint64_t now)
{
    if (!hal::hasBattery() || (batteryKnown && now - lastBatteryReadMs < kBatteryReadMs)) {
        return;
    }
    lastBatteryReadMs = now;
    battery = hal::readBattery();
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
    bar.hasBattery = hal::hasBattery();
    bar.batteryKnown = batteryKnown;
    bar.externalPower = battery.external;
    bar.batteryPercent = battery.percent;
    return bar;
}

// Arduino's setCpuFrequencyMhz() uses the fast switch, which on the ESP32-S3
// with octal PSRAM froze the M5StickS3 at 80 MHz (BOARDS.md Q-012). The full
// switch reconfigures the PLL as well; measured by cycle count at 80 and 160 MHz.
// APB stays at 80 MHz at both clocks, so no peripheral needs reconfiguring.
void setCpuMhz(uint32_t mhz)
{
    rtc_cpu_freq_config_t config;
    if (!rtc_clk_cpu_freq_mhz_to_config(mhz, &config)) {
        Serial.printf("CPU clock %u MHz not supported\n", static_cast<unsigned>(mhz));
        return;
    }
    rtc_clk_cpu_freq_set_config(&config);
}

// Applies a screen saver phase change (F-014). Returns true if the message
// screen is back and must be redrawn.
bool enterSaverPhase(ui::SaverPhase phase, uint64_t now)
{
    if (phase == saverPhase) {
        return false;
    }
    const ui::SaverPhase previous = saverPhase;
    saverPhase = phase;
    switch (phase) {
    case ui::SaverPhase::Awake:
        screen::wake();
        setCpuMhz(kCpuMhz);
        Serial.printf("Screen saver off, CPU %u MHz\n", static_cast<unsigned>(getCpuFrequencyMhz()));
        return true;
    case ui::SaverPhase::Dark:
        if (previous == ui::SaverPhase::Animating && particleFrames > 0) {
            Serial.printf("Screen saver: %u frames, %u ms per frame to draw at %u MHz\n",
                          static_cast<unsigned>(particleFrames),
                          static_cast<unsigned>(particleRenderUs / particleFrames / 1000),
                          static_cast<unsigned>(getCpuFrequencyMhz()));
        }
        screen::sleep();
        setCpuMhz(kSaverCpuMhz);
        if (previous == ui::SaverPhase::Awake) {
            Serial.printf("Screen saver on, CPU %u MHz\n", static_cast<unsigned>(getCpuFrequencyMhz()));
        }
        return false;
    case ui::SaverPhase::Animating:
        particles.start(board::kScreenWidth, board::kScreenHeight, esp_random());
        clock_sync::text(saverClock, sizeof(saverClock));
        clock_sync::dateText(saverDate, sizeof(saverDate));
        if (saverDate[0] == '\0') {
            saverClock[0] = '\0';  // "--:--": no clock, particles only
        }
        saverGathering = false;
        saverBurst = false;
        saverSlowed = false;
        setCpuMhz(kSaverAnimationCpuMhz);
        screen::wake();
        lastParticleFrameMs = now;
        particleFrames = 0;
        particleRenderUs = 0;
        return false;
    }
    return false;
}

// Gathers the particles for the clock and bursts them away again; returns the
// text and glow for this frame.
screen::SaverText runClockShow(uint32_t elapsedMs, uint8_t level)
{
    screen::SaverText text{saverClock, saverDate, 0, 0};
    if (saverClock[0] == '\0') {
        return text;
    }
    constexpr float kCenterX = board::kScreenWidth / 2.0f;
    constexpr float kCenterY = board::kScreenHeight / 2.0f;
    if (!saverGathering && elapsedMs >= kSaverGatherMs) {
        saverGathering = true;
        particles.gather(kCenterX, kCenterY);
    }
    if (!saverBurst && elapsedMs >= kSaverBurstMs) {
        saverBurst = true;
        particles.burst(kCenterX, kCenterY, kSaverBurstBoost);
    }
    if (elapsedMs < kSaverTextMs) {
        return text;
    }
    if (!saverSlowed && !saverBurst) {
        saverSlowed = true;
        particles.setPace(kSaverGlowPace);
    }
    // The glow follows the text: in while it fades in, out while it fades out.
    const uint8_t shown = ui::fadeLevel(elapsedMs - kSaverTextMs, kSaverTextLengthMs, kSaverTextRampMs,
                                        screen::kParticleLevels - 1);
    text.level = shown < level ? shown : level;
    text.glow = shown;
    return text;
}

void runAnimation(uint64_t now)
{
    if (now - lastParticleFrameMs < kSaverFrameMs) {
        return;
    }
    const uint32_t elapsedMs = static_cast<uint32_t>(now - saver.animationStartMs());
    const uint8_t level = ui::fadeLevel(elapsedMs, kSaverAnimationMs, kSaverFadeMs, screen::kParticleLevels - 1);
    const screen::SaverText text = runClockShow(elapsedMs, level);
    particles.step(static_cast<float>(now - lastParticleFrameMs) / 1000.0f);
    lastParticleFrameMs = now;
    const int64_t startUs = esp_timer_get_time();
    screen::drawParticles(particles, level, text);
    particleRenderUs += static_cast<uint64_t>(esp_timer_get_time() - startUs);
    ++particleFrames;
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

// The sound for a show_message outcome (F-013).
void playChimeFor(mcp::Sound sound)
{
    switch (sound) {
    case mcp::Sound::NewMessage:
        hal::playChime(hal::Chime::NewMessage);
        break;
    case mcp::Sound::Queued:
        hal::playChime(hal::Chime::Queued);
        break;
    case mcp::Sound::Timed:
        hal::playChime(hal::Chime::Timed);
        break;
    case mcp::Sound::Rejected:
        hal::playChime(hal::Chime::Rejected);
        break;
    case mcp::Sound::None:
        break;
    }
}

}  // namespace

void setup()
{
    // Before Serial: the UART clock (APB, 80 MHz) is the same at 160 and 80 MHz.
    setCpuMhz(kCpuMhz);
    Serial.begin(115200);
    Serial.printf("McpVue v%s on %s, CPU %u MHz\n", FW_VERSION, board::kName,
                  static_cast<unsigned>(getCpuFrequencyMhz()));
    hal::begin();
    Serial.printf("Hardware: %s\n", hal::description());


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
    handleSerialCommands(now);
    // Buttons are read in every mode so their state stays consistent; events
    // are ignored while the setup or welcome screen is shown.
    ui::PairEvent event = buttons.update(hal::deletePressed(), hal::scrollPressed(), now);
    const bool inputActive = !buttons.idle();
    // A press while the screen saver runs only wakes the screen (F-014): its
    // events, including the release, are dropped until the inputs settle.
    if (saverPhase != ui::SaverPhase::Awake && inputActive) {
        swallowPress = true;
    }
    if (swallowPress) {
        event = ui::PairEvent::None;
        swallowPress = inputActive;
    }

    if (network::state() == network::State::Setup) {
        saver.update(now, false, false);
        enterSaverPhase(ui::SaverPhase::Awake, now);
        runSetupScreen();
        saveIfDue(now);
        delay(20);
        return;
    }
    setupShown = false;

    bool contentChanged = announceConnection(now);
    const bool covered = now < coverUntilMs;
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

    const bool idle = !covered && queue.empty();
    redraw |= enterSaverPhase(saver.update(now, idle, inputActive), now);
    // After the wake: its CPU clock switch restarts the PLL that clocks I2S,
    // which distorted a chime already playing (F-013, F-014).
    if (hal::hasSpeaker()) {
        playChimeFor(mcpEvents.sound);  // every show_message, replacements and a full queue included (F-013)
    }
    if (saverPhase == ui::SaverPhase::Animating) {
        runAnimation(now);
    } else if (saverPhase == ui::SaverPhase::Awake && !covered) {
        redraw |= coverShown;
        coverShown = false;
        readBatteryIfDue(now);
        char arrival[24] = "";
        if (!queue.empty()) {
            clock_sync::arrivalText(queue.current()->receivedAt, arrival, sizeof(arrival));
        }
        screen::update(queue, now, redraw, statusBar(), arrival);
    }
    delay(5);
}
