#include "es8311_chime.h"

#include <Arduino.h>
#include <driver/i2s.h>

namespace es8311_chime {

namespace {

// ES8311 as I2S slave, DAC only, 16 kHz, 16-bit, MCLK 256 x fs = 4.096 MHz
// from the ESP32-S3. Register values follow Espressif's es8311 driver
// (Apache-2.0) for that clock; the codec's datasheet names them.
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
struct TuneData {
    const Note* notes;
    int noteCount;
    uint32_t ms;
};
// Two bell-like notes, high then low (E6, C6), overlapping.
constexpr Note kNewMessageNotes[] = {{1318.5f, 0, 110}, {1046.5f, 140, 170}};
// The same bell, one pitch, a quicker double tap (C6, C6).
constexpr Note kQueuedNotes[] = {{1046.5f, 0, 70}, {1046.5f, 130, 110}};
// One short high ping (G6).
constexpr Note kTimedNotes[] = {{1568.0f, 0, 90}};
// Low and falling (C5, A4), heard as an error next to the others.
constexpr Note kRejectedNotes[] = {{523.3f, 0, 120}, {440.0f, 180, 200}};
// Indexed by Tune.
constexpr TuneData kTunes[] = {
    {kNewMessageNotes, 2, 750},
    {kQueuedNotes, 2, 550},
    {kTimedNotes, 1, 400},
    {kRejectedNotes, 2, 850},
};

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

Config settings{};
QueueHandle_t pendingTunes = nullptr;

bool writeRegister(uint8_t reg, uint8_t value)
{
    settings.wire->beginTransmission(settings.address);
    settings.wire->write(reg);
    settings.wire->write(value);
    return settings.wire->endTransmission() == 0;
}

bool readRegister(uint8_t reg, uint8_t& value)
{
    settings.wire->beginTransmission(settings.address);
    settings.wire->write(reg);
    if (settings.wire->endTransmission(false) != 0 ||
        settings.wire->requestFrom(settings.address, static_cast<uint8_t>(1)) != 1) {
        return false;
    }
    value = settings.wire->read();
    return true;
}

bool beginCodec()
{
    if (!writeRegister(0x00, 0x1F)) {  // reset
        return false;
    }
    delay(20);
    if (!writeRegister(0x00, 0x00) || !writeRegister(0x00, 0x80)) {  // power on
        return false;
    }
    for (const CodecStep& step : kCodecSetup) {
        uint8_t value = step.value;
        if (step.keep != 0) {
            uint8_t old = 0;
            if (!readRegister(step.reg, old)) {
                return false;
            }
            value |= old & step.keep;
        }
        if (!writeRegister(step.reg, value)) {
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
    pins.mck_io_num = settings.pinMclk;
    pins.bck_io_num = settings.pinBclk;
    pins.ws_io_num = settings.pinWs;
    pins.data_out_num = settings.pinDout;
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

// Writes a tune to I2S, blocking this task only. The amplifier is on only
// while it plays, so it does not hiss in between.
void playNow(const TuneData& tune)
{
    static int16_t frames[kChimeFrames * 2];  // stereo, both channels the same
    const uint32_t total = kSampleRate * tune.ms / 1000;
    constexpr float kAttackMs = 5;  // fade in, against clicks
    settings.setAmplifier(true);
    delay(5);
    for (uint32_t start = 0; start < total; start += kChimeFrames) {
        for (int i = 0; i < kChimeFrames; i++) {
            const uint32_t n = start + i;
            float sample = 0;
            for (int k = 0; k < tune.noteCount; k++) {
                const Note& note = tune.notes[k];
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
    settings.setAmplifier(false);
}

void chimeLoop(void*)
{
    for (;;) {
        Tune tune;
        if (xQueueReceive(pendingTunes, &tune, portMAX_DELAY) == pdTRUE) {
            playNow(kTunes[static_cast<int>(tune)]);
        }
    }
}

}  // namespace

bool begin(const Config& config)
{
    settings = config;
    settings.setAmplifier(false);
    if (!beginI2s()) {
        return false;
    }
    // The codec needs MCLK running before its clock registers take effect.
    if (!beginCodec()) {
        i2s_driver_uninstall(kI2sPort);
        return false;
    }
    pendingTunes = xQueueCreate(kMaxPending, sizeof(Tune));
    if (pendingTunes == nullptr) {
        return false;
    }
    // Core 1 with the loop task, one priority above it: generating samples is
    // quick, then the task waits on I2S.
    if (xTaskCreatePinnedToCore(chimeLoop, "chime", 3072, nullptr, 2, nullptr, 1) != pdPASS) {
        vQueueDelete(pendingTunes);
        pendingTunes = nullptr;
        return false;
    }
    return true;
}

void play(Tune tune)
{
    if (pendingTunes != nullptr) {
        xQueueSend(pendingTunes, &tune, 0);  // never blocks the caller; dropped when full
    }
}

}  // namespace es8311_chime
