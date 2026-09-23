// Wi-Fi connection with ESP BLE Provisioning (PROJECT.md F-002, D-002, D-023).
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace network {

enum class State {
    Setup,       // no Wi-Fi settings: BLE provisioning is running
    Connecting,  // settings present, waiting for an IP address
    Connected,   // got an IP address from DHCP
    Offline,     // settings present, network lost; reconnecting in the background
};

enum class SetupStatus {
    Waiting,        // waiting for the phone app
    Connecting,     // credentials received, trying them
    WrongPassword,  // last attempt failed: authentication error
    NotFound,       // last attempt failed: network not found
};

// Starts Wi-Fi: connects with saved settings, or starts BLE provisioning.
void begin();

// Retries the connection while offline. Call every loop.
void poll(uint64_t nowMs);

State state();
SetupStatus setupStatus();

// Device name and proof-of-possession code shown on the setup screen.
const char* serviceName();
const char* pop();
// JSON payload for the ESP BLE Provisioning app's QR scanner.
const char* qrPayload();

// Writes the IP address, or a short status, for the top bar.
void label(char* out, size_t size);

// Forgets the saved Wi-Fi settings and restarts into setup.
void resetAndRestart();

}  // namespace network
