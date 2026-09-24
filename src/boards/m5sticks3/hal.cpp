// M5Stack M5StickS3 (BOARDS.md, PROJECT.md D-032). The M5PM1 power chip on
// I2C switches the display supply and the speaker amplifier and measures the
// battery; register use follows M5Stack's M5GFX and M5Unified.
#include "hal.h"

#include <U8g2lib.h>
#include <Wire.h>
#include <battery_level.h>
#include <es8311_chime.h>

#include "board.h"

namespace hal {

namespace {

// USB serial write timeout; must not be 0 (BOARDS.md Q-008).
constexpr uint32_t kSerialTimeoutMs = 10;
constexpr uint32_t kSerialBulkTimeoutMs = 1000;

constexpr uint32_t kI2cHz = 100000;  // the M5PM1's I2C speed in M5Stack's libraries

// M5PM1 registers
constexpr uint8_t kPm1DeviceId = 0x00;
constexpr uint8_t kPm1I2cConfig = 0x09;  // 0: no I2C idle sleep
constexpr uint8_t kPm1Watchdog = 0x0A;   // 0: watchdog off
constexpr uint8_t kPm1GpioMode = 0x10;   // 1 = output
constexpr uint8_t kPm1GpioOut = 0x11;
constexpr uint8_t kPm1GpioDrive = 0x13;  // 0 = push-pull
constexpr uint8_t kPm1GpioFunc = 0x16;   // 0 = GPIO function (PM1 GPIO0 to GPIO3)
constexpr uint8_t kPm1BatteryMv = 0x22;  // 16-bit little endian
constexpr uint8_t kPm1InputMv = 0x24;    // USB input, 16-bit little endian

constexpr uint8_t kPm1LcdPower = 1 << 2;  // PM1 GPIO2: display supply
constexpr uint8_t kPm1Amplifier = 1 << 3;  // PM1 GPIO3: speaker amplifier enable

// Above this on the USB input the board runs from USB.
constexpr uint32_t kUsbPresentMv = 4000;

bool pm1Ready = false;
bool speakerReady = false;
char descriptionText[96];

bool writeRegister(uint8_t address, uint8_t reg, uint8_t value)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readRegisters(uint8_t address, uint8_t reg, uint8_t* values, uint8_t count)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address, count) != count) {
        return false;
    }
    for (uint8_t i = 0; i < count; i++) {
        values[i] = Wire.read();
    }
    return true;
}

// Sets (on) or clears the bits in mask of an M5PM1 register.
bool pm1Bits(uint8_t reg, uint8_t mask, bool on)
{
    uint8_t value = 0;
    if (!readRegisters(I2C_ADDR_M5PM1, reg, &value, 1)) {
        return false;
    }
    return writeRegister(I2C_ADDR_M5PM1, reg, on ? (value | mask) : (value & ~mask));
}

// A PM1 pin as a push-pull output at the given level.
bool pm1Output(uint8_t mask, bool high)
{
    return pm1Bits(kPm1GpioFunc, mask, false) && pm1Bits(kPm1GpioMode, mask, true) &&
           pm1Bits(kPm1GpioDrive, mask, false) && pm1Bits(kPm1GpioOut, mask, high);
}

uint32_t pm1Millivolts(uint8_t reg)
{
    uint8_t bytes[2] = {};
    return readRegisters(I2C_ADDR_M5PM1, reg, bytes, 2) ? (bytes[1] << 8 | bytes[0]) : 0;
}

// Called from the chime task.
void setAmplifier(bool on)
{
    pm1Bits(kPm1GpioOut, kPm1Amplifier, on);
}

void beginPowerChip()
{
    uint8_t id = 0;
    if (!readRegisters(I2C_ADDR_M5PM1, kPm1DeviceId, &id, 1)) {
        return;
    }
    // As in M5Unified: the chip keeps its settings while the battery is
    // connected, so set idle sleep and the watchdog explicitly.
    writeRegister(I2C_ADDR_M5PM1, kPm1I2cConfig, 0x00);
    writeRegister(I2C_ADDR_M5PM1, kPm1Watchdog, 0x00);
    pm1Ready = pm1Output(kPm1LcdPower, true) && pm1Output(kPm1Amplifier, false);
    delay(100);  // display supply settles before the panel starts
}

}  // namespace

void begin()
{
#if ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(kSerialTimeoutMs);
#endif
    pinMode(PIN_BUTTON_DELETE, INPUT_PULLUP);
    pinMode(PIN_BUTTON_SCROLL, INPUT_PULLUP);
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, kI2cHz);
    beginPowerChip();
    if (pm1Ready) {
        speakerReady = es8311_chime::begin(es8311_chime::Config{&Wire, I2C_ADDR_ES8311, PIN_I2S_MCLK, PIN_I2S_BCLK,
                                                                PIN_I2S_WS, PIN_I2S_DOUT, setAmplifier});
    }
    // The backlight is a plain GPIO, active HIGH.
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    snprintf(descriptionText, sizeof(descriptionText), "ST7789P3 240 x 135, 2 buttons, M5PM1 %s, ES8311 %s",
             pm1Ready ? "ok" : "missing", speakerReady ? "ok" : "missing");
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

// ST7789P3, 135 x 240, on SPI. Like the T-Display's panel it sits off-center
// in the controller's 240 x 320 memory (offsets from M5GFX).
Arduino_GFX* display()
{
    static Arduino_DataBus* bus =
        new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI, GFX_NOT_DEFINED, FSPI, false);
    static Arduino_GFX* panel =
        new Arduino_ST7789(bus, TFT_RST, board::kScreenRotation, true, 135, 240, 52, 40, 53, 40);
    return panel;
}

int32_t displaySpeedHz()
{
    return 40000000;
}

// ASCII u8g2 fonts, sized for 240 x 135 on 1.14 inch (as on the T-Display).
Fonts fonts()
{
    return Fonts{u8g2_font_helvR12_tr, u8g2_font_helvB12_tr, u8g2_font_helvR12_tr, u8g2_font_helvR18_tr};
}

bool deletePressed()
{
    return digitalRead(PIN_BUTTON_DELETE) == LOW;
}

bool scrollPressed()
{
    return digitalRead(PIN_BUTTON_SCROLL) == LOW;
}

bool hasSpeaker()
{
    return speakerReady;
}

void playChime()
{
    es8311_chime::play();
}

bool hasBattery()
{
    return pm1Ready;
}

Battery readBattery()
{
    const uint32_t cellMv = pm1Millivolts(kPm1BatteryMv);
    const ui::BatteryLevel level = ui::batteryLevel(cellMv);
    const bool usb = pm1Millivolts(kPm1InputMv) >= kUsbPresentMv;
    return Battery{usb || level.external, level.percent, cellMv};
}

}  // namespace hal
