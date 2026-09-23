#include "network.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiProv.h>
#include <esp_mac.h>

namespace network {

namespace {

// Easy to read and type: no 0/o, 1/l/i.
constexpr char kPopAlphabet[] = "abcdefghjkmnpqrstuvwxyz23456789";
constexpr size_t kPopLength = 8;
constexpr uint32_t kRetryMs = 15000;

char name[16];
char popCode[kPopLength + 1];
char payload[128];
uint64_t lastRetryMs = 0;

// Written by the Wi-Fi event task, read by the loop task. Single enum stores
// are atomic on the ESP32, so volatile is enough.
volatile State currentState = State::Connecting;
volatile SetupStatus currentSetupStatus = SetupStatus::Waiting;

void onEvent(arduino_event_t* event)
{
    switch (event->event_id) {
    case ARDUINO_EVENT_PROV_START:
        currentState = State::Setup;
        currentSetupStatus = SetupStatus::Waiting;
        Serial.printf("Wi-Fi setup: waiting for the app, device %s\n", name);
        break;
    case ARDUINO_EVENT_PROV_CRED_RECV:
        currentSetupStatus = SetupStatus::Connecting;
        Serial.println("Wi-Fi setup: credentials received");
        break;
    case ARDUINO_EVENT_PROV_CRED_FAIL:
        currentSetupStatus = event->event_info.prov_fail_reason == WIFI_PROV_STA_AUTH_ERROR ? SetupStatus::WrongPassword
                                                                                             : SetupStatus::NotFound;
        Serial.println("Wi-Fi setup: connection with the received credentials failed");
        break;
    case ARDUINO_EVENT_PROV_END:
        Serial.println("Wi-Fi setup: finished, BLE released");
        break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        currentState = State::Connected;
        Serial.printf("Wi-Fi: connected to %s, IP %s\n", WiFi.SSID().c_str(),
                      IPAddress(event->event_info.got_ip.ip_info.ip.addr).toString().c_str());
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        // During setup a failed attempt also disconnects; the setup status reports it instead.
        if (currentState != State::Setup) {
            if (currentState == State::Connected) {
                Serial.printf("Wi-Fi: disconnected, reason %u\n", event->event_info.wifi_sta_disconnected.reason);
            }
            currentState = State::Offline;
        }
        break;
    default:
        break;
    }
}

}  // namespace

void begin()
{
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    // Same naming as the Espressif examples; the app lists devices starting with PROV_.
    snprintf(name, sizeof(name), "PROV_%02X%02X%02X", mac[3], mac[4], mac[5]);
    for (size_t i = 0; i < kPopLength; i++) {
        popCode[i] = kPopAlphabet[esp_random() % (sizeof(kPopAlphabet) - 1)];
    }
    popCode[kPopLength] = '\0';
    snprintf(payload, sizeof(payload), "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"ble\"}", name,
             popCode);

    WiFi.onEvent(onEvent);
    // FREE_BTDM releases the BLE stack's memory once provisioning ends, or right
    // away when the device is already provisioned.
    WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM, WIFI_PROV_SECURITY_1, popCode,
                            name);
}

void poll(uint64_t nowMs)
{
    if (currentState == State::Offline && nowMs - lastRetryMs >= kRetryMs) {
        lastRetryMs = nowMs;
        Serial.println("Wi-Fi: reconnecting");
        WiFi.reconnect();
    }
}

State state()
{
    return currentState;
}

SetupStatus setupStatus()
{
    return currentSetupStatus;
}

const char* serviceName()
{
    return name;
}

const char* pop()
{
    return popCode;
}

const char* qrPayload()
{
    return payload;
}

void label(char* out, size_t size)
{
    switch (currentState) {
    case State::Connected:
        snprintf(out, size, "%s", WiFi.localIP().toString().c_str());
        return;
    case State::Connecting:
        snprintf(out, size, "connecting");
        return;
    case State::Setup:
        snprintf(out, size, "setup");
        return;
    case State::Offline:
        break;
    }
    snprintf(out, size, "no network");
}

void resetAndRestart()
{
    Serial.println("Wi-Fi: forgetting settings and restarting");
    WiFi.disconnect(true, true);  // wifioff, erase the saved network
    delay(200);
    ESP.restart();
}

}  // namespace network

// Arduino's weak btInUse() returns false unless the core's BT helpers are linked,
// and then releases the BLE controller memory at boot, before provisioning can
// start it (BOARDS.md Q-005). Returning true keeps it; the provisioning manager
// releases it itself when setup ends or is not needed (FREE_BTDM).
extern "C" bool btInUse()
{
    return true;
}
