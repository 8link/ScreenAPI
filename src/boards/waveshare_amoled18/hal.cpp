// Waveshare ESP32-S3-Touch-AMOLED-1.8 (BOARDS.md, PROJECT.md D-032).
// Two revisions: original (SH8601 display, FT3168 touch) and V2 (CO5300, CST820).
// The touch chip's I2C address tells them apart.
#include "hal.h"

#include <Arduino_SH8601.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <XPowersLib.h>
#include <driver/i2s.h>

#include "board.h"

namespace hal {

namespace {

// USB serial write timeout. Must not be 0: arduino-esp32 2.0.17 HWCDC::write()
// counts retries with "uint32_t tries = tx_timeout_ms; tries--", which wraps at
// 0, so with a PC connected but no program reading the port the loop waited
// about 49 days, 1 ms at a time (BOARDS.md Q-008). With 10 ms it gives up, marks
// the port disconnected, and later output is dropped instead of blocking.
constexpr uint32_t kSerialTimeoutMs = 10;
constexpr uint32_t kSerialBulkTimeoutMs = 1000;

// TCA9554 registers
constexpr uint8_t kExpanderOutput = 0x01;
constexpr uint8_t kExpanderConfig = 0x03;  // 1 = input
constexpr uint8_t kResetPins = 0x07;       // pins 0 to 2

// Touch registers: 0x02 holds the number of touch points on both chips.
constexpr uint8_t kTouchPoints = 0x02;
constexpr uint8_t kCst820AutoSleep = 0xFE;  // write 1 to stay awake (CST816 family)

enum class Revision { Unknown, Original, V2 };

Revision revision = Revision::Unknown;
uint8_t touchAddress = 0;
XPowersAXP2101 pmu;
bool pmuReady = false;
bool speakerReady = false;
TaskHandle_t chimeTask = nullptr;
char descriptionText[128];

bool probe(uint8_t address)
{
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readRegister(uint8_t address, uint8_t reg, uint8_t& value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) {
        return false;
    }
    value = Wire.read();
    return true;
}

// The display and touch reset lines hang off the expander; pulse them low for
// 20 ms as Waveshare's examples do.
bool resetDisplayAndTouch()
{
    if (!writeRegister(I2C_ADDR_EXPANDER, kExpanderConfig, static_cast<uint8_t>(~kResetPins)) ||
        !writeRegister(I2C_ADDR_EXPANDER, kExpanderOutput, 0x00)) {
        return false;
    }
    delay(20);
    return writeRegister(I2C_ADDR_EXPANDER, kExpanderOutput, kResetPins);
}

// The touch controller needs time after reset before it answers; try for up to 1 s.
void detectRevision()
{
    for (int attempt = 0; attempt < 20 && revision == Revision::Unknown; attempt++) {
        delay(50);
        if (probe(I2C_ADDR_FT3168)) {
            revision = Revision::Original;
            touchAddress = I2C_ADDR_FT3168;
        } else if (probe(I2C_ADDR_CST820)) {
            revision = Revision::V2;
            touchAddress = I2C_ADDR_CST820;
        }
    }
    if (revision == Revision::V2) {
        // Unverified for the CST820: CST816-family chips stop answering on I2C
        // while asleep, which would make polling miss the first touch.
        writeRegister(touchAddress, kCst820AutoSleep, 0x01);
    }
}

void beginPowerChip()
{
    pmuReady = pmu.begin(Wire, I2C_ADDR_AXP2101, PIN_I2C_SDA, PIN_I2C_SCL);
    if (pmuReady) {
        pmu.enableBattDetection();
        pmu.enableBattVoltageMeasure();
        pmu.enableVbusVoltageMeasure();
    }
}

// Chime (F-013): ES8311 codec as I2S slave, DAC only, 16 kHz, 16-bit, MCLK
// 256 x fs = 4.096 MHz from the ESP32-S3. Register values follow Espressif's
// es8311 driver (Apache-2.0) for that clock; the codec's datasheet names them.
constexpr uint32_t kSampleRate = 16000;
constexpr i2s_port_t kI2sPort = I2S_NUM_0;
constexpr uint8_t kDacVolume = 0xBF;     // register 0x32: 0xBF is 0 dB, 0.5 dB per step
constexpr float kChimeAmplitude = 9000;  // peak sample value of each note, of 32767
constexpr int kChimeFrames = 256;        // samples per write

struct Note {
    float hz;
    uint32_t startMs;
    float decayMs;  // time for the note to fall to 1/e
};
// Two bell-like notes, high then low (E6, C6), overlapping.
constexpr Note kChimeNotes[] = {{1318.5f, 0, 110}, {1046.5f, 140, 170}};
constexpr uint32_t kChimeMs = 750;

// One register step: new value = (old & keep) | value; keep 0 writes value as is.
struct CodecStep {
    uint8_t reg;
    uint8_t keep;
    uint8_t value;
};

constexpr CodecStep kCodecSetup[] = {
    {0x01, 0x00, 0x3F},  // clocks on, MCLK from the MCLK pin
    {0x06, 0xC0, 0x03},  // BCLK not inverted, BCLK = MCLK / 4
    {0x02, 0x07, 0x00},  // pre-divider 1, multiplier 1
    {0x03, 0x00, 0x10},  // single speed, ADC oversampling
    {0x04, 0x00, 0x10},  // DAC oversampling
    {0x05, 0x00, 0x00},  // ADC and DAC dividers 1
    {0x07, 0xC0, 0x00},  // LRCK divider 0x00FF (with 0x08): 4.096 MHz / 256
    {0x08, 0x00, 0xFF},
    {0x00, 0xBF, 0x00},  // I2S slave
    {0x09, 0x00, 0x0C},  // serial in: I2S, 16-bit
    {0x0A, 0x00, 0x0C},  // serial out: I2S, 16-bit
    {0x0D, 0x00, 0x01},  // analog power up
    {0x0E, 0x00, 0x02},  // PGA and ADC modulator
    {0x12, 0x00, 0x00},  // DAC power up
    {0x13, 0x00, 0x10},  // output driver on
    {0x1C, 0x00, 0x6A},  // ADC equalizer bypass
    {0x37, 0x00, 0x08},  // DAC equalizer bypass
    {0x32, 0x00, kDacVolume},
    {0x31, 0x9F, 0x00},  // DAC unmuted
};

bool beginCodec()
{
    constexpr uint8_t address = I2C_ADDR_ES8311;
    if (!writeRegister(address, 0x00, 0x1F)) {  // reset
        return false;
    }
    delay(20);
    if (!writeRegister(address, 0x00, 0x00) || !writeRegister(address, 0x00, 0x80)) {  // power on
        return false;
    }
    for (const CodecStep& step : kCodecSetup) {
        uint8_t value = step.value;
        if (step.keep != 0) {
            uint8_t old = 0;
            if (!readRegister(address, step.reg, old)) {
                return false;
            }
            value |= old & step.keep;
        }
        if (!writeRegister(address, step.reg, value)) {
            return false;
        }
    }
    return true;
}

bool beginI2s()
{
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
    config.sample_rate = kSampleRate;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.dma_buf_count = 4;
    config.dma_buf_len = kChimeFrames;
    config.tx_desc_auto_clear = true;  // silence, not the last buffer again, when idle
    config.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    i2s_pin_config_t pins = {};
    pins.mck_io_num = PIN_I2S_MCLK;
    pins.bck_io_num = PIN_I2S_BCLK;
    pins.ws_io_num = PIN_I2S_WS;
    pins.data_out_num = PIN_I2S_DOUT;
    pins.data_in_num = I2S_PIN_NO_CHANGE;
    if (i2s_driver_install(kI2sPort, &config, 0, nullptr) != ESP_OK) {
        return false;
    }
    if (i2s_set_pin(kI2sPort, &pins) != ESP_OK) {
        i2s_driver_uninstall(kI2sPort);
        return false;
    }
    return true;
}

// Writes the chime to I2S, blocking this task only. The amplifier is on only
// while it plays, so it does not hiss in between.
void playChimeNow()
{
    static int16_t frames[kChimeFrames * 2];  // stereo, both channels the same
    constexpr uint32_t total = kSampleRate * kChimeMs / 1000;
    constexpr float kAttackMs = 5;  // fade in, against clicks
    digitalWrite(PIN_AMP_EN, HIGH);
    delay(5);
    for (uint32_t start = 0; start < total; start += kChimeFrames) {
        for (int i = 0; i < kChimeFrames; i++) {
            const uint32_t n = start + i;
            float sample = 0;
            for (const Note& note : kChimeNotes) {
                const float ms = n * 1000.0f / kSampleRate - note.startMs;
                if (ms < 0) {
                    continue;
                }
                const float fade = ms < kAttackMs ? ms / kAttackMs : 1.0f;
                sample += kChimeAmplitude * fade * expf(-ms / note.decayMs) * sinf(2 * PI * note.hz * ms / 1000);
            }
            const int16_t value = n < total ? static_cast<int16_t>(sample) : 0;
            frames[2 * i] = value;
            frames[2 * i + 1] = value;
        }
        size_t written = 0;
        i2s_write(kI2sPort, frames, sizeof(frames), &written, portMAX_DELAY);
    }
    // Let the DMA buffers play out before the amplifier goes off.
    delay(4 * kChimeFrames * 1000 / kSampleRate + 10);
    digitalWrite(PIN_AMP_EN, LOW);
}

void chimeLoop(void*)
{
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        playChimeNow();
        ulTaskNotifyTake(pdTRUE, 0);  // drop requests made while playing
    }
}

void beginSpeaker()
{
    pinMode(PIN_AMP_EN, OUTPUT);
    digitalWrite(PIN_AMP_EN, LOW);
    if (!beginI2s()) {
        return;
    }
    // The codec needs MCLK running before its clock registers take effect.
    if (!beginCodec()) {
        i2s_driver_uninstall(kI2sPort);
        return;
    }
    // Core 1 with the loop task, one priority above it: generating samples is
    // quick, then the task waits on I2S.
    speakerReady = xTaskCreatePinnedToCore(chimeLoop, "chime", 3072, nullptr, 2, &chimeTask, 1) == pdPASS;
}

}  // namespace

void begin()
{
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(kSerialTimeoutMs);
#endif
    pinMode(PIN_BUTTON_DELETE, INPUT_PULLUP);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 400000);
    const bool expanderOk = resetDisplayAndTouch();
    detectRevision();
    beginPowerChip();
    beginSpeaker();
    const char* chips = revision == Revision::Original ? "SH8601 + FT3168 (original)"
                        : revision == Revision::V2     ? "CO5300 + CST820 (V2)"
                                                       : "touch not found, assuming CO5300 (V2)";
    snprintf(descriptionText, sizeof(descriptionText), "%s, expander %s, AXP2101 %s, ES8311 %s", chips,
             expanderOk ? "ok" : "missing", pmuReady ? "ok" : "missing", speakerReady ? "ok" : "missing");
}

// Native USB serial (HWCDC): with the short timeout, output that does not fit
// the 256-byte transmit buffer is dropped; a screenshot waits for the host.
void setSerialBlocking(bool blocking)
{
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(blocking ? kSerialBulkTimeoutMs : kSerialTimeoutMs);
#endif
}

const char* description()
{
    return descriptionText;
}

Arduino_GFX* display()
{
    static Arduino_DataBus* bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
    // Constructor arguments follow Waveshare's examples: no reset pin (expander),
    // 368 x 448, and a 16 px column offset on the CO5300.
    static Arduino_GFX* panel =
        revision == Revision::Original
            ? static_cast<Arduino_GFX*>(
                  new Arduino_SH8601(bus, GFX_NOT_DEFINED, board::kScreenRotation, false, 368, 448))
            : static_cast<Arduino_GFX*>(
                  new Arduino_CO5300(bus, GFX_NOT_DEFINED, board::kScreenRotation, false, 368, 448, 16, 0, 0, 0));
    return panel;
}

int32_t displaySpeedHz()
{
    return GFX_NOT_DEFINED;  // the QSPI bus default
}

// ASCII u8g2 fonts, sized for 368 x 448 on 1.8 inch (about 330 ppi).
Fonts fonts()
{
    return Fonts{u8g2_font_helvR14_tr, u8g2_font_fub20_tr, u8g2_font_helvR18_tr, u8g2_font_helvR24_tr};
}

bool deletePressed()
{
    return digitalRead(PIN_BUTTON_DELETE) == LOW;
}

// The touchscreen acts as the scroll button: pressed while a finger is down.
bool scrollPressed()
{
    uint8_t points = 0;
    if (touchAddress == 0 || !readRegister(touchAddress, kTouchPoints, points)) {
        return false;
    }
    points &= 0x0F;
    return points > 0 && points <= 5;
}

bool hasSpeaker()
{
    return speakerReady;
}

void playChime()
{
    xTaskNotifyGive(chimeTask);
}

bool hasBattery()
{
    return pmuReady;
}

Battery readBattery()
{
    const bool usb = pmu.isVbusIn();
    const bool cell = pmu.isBatteryConnect();
    const int percent = cell ? pmu.getBatteryPercent() : 0;
    return Battery{usb || !cell, percent < 0 ? 0 : percent, cell ? static_cast<uint32_t>(pmu.getBattVoltage()) : 0};
}

}  // namespace hal
