#include <Arduino.h>
#include <TFT_eSPI.h>

static TFT_eSPI tft;

static void drawBootScreen()
{
    const int centerX = tft.width() / 2;
    const int centerY = tft.height() / 2;

    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("ScreenAPI", centerX, centerY - 14, 4);
    tft.drawString("v" FW_VERSION, centerX, centerY + 16, 2);
}

void setup()
{
    Serial.begin(115200);
    Serial.printf("ScreenAPI v%s\n", FW_VERSION);

    tft.init();          // also switches the backlight on (TFT_BL in tft_setup.h)
    tft.setRotation(1);  // landscape, 240 x 135
    drawBootScreen();
}

void loop()
{
    delay(1000);
}
