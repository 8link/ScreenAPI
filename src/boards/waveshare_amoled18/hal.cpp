// Waveshare ESP32-S3-Touch-AMOLED-1.8 (BOARDS.md, PROJECT.md D-032).
// Two revisions: original (SH8601 display, FT3168 touch) and V2 (CO5300, CST820).
// The touch chip's I2C address tells them apart.
#include "hal.h"

#include <Arduino_SH8601.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <XPowersLib.h>

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
char descriptionText[96];

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
    const char* chips = revision == Revision::Original ? "SH8601 + FT3168 (original)"
                        : revision == Revision::V2     ? "CO5300 + CST820 (V2)"
                                                       : "touch not found, assuming CO5300 (V2)";
    snprintf(descriptionText, sizeof(descriptionText), "%s, expander %s, AXP2101 %s", chips,
             expanderOk ? "ok" : "missing", pmuReady ? "ok" : "missing");
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
