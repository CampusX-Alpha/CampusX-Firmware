// secrets.example.h copy to secrets.h and fill with real values.
// secrets.h is gitignored and never committed.
#pragma once

#define WIFI_SSID "CAMPUS_WIFI_SSID"
#define WIFI_PASS "CAMPUS_WIFI_PASS"

// Set to host machine LAN IP so ESP32s on Wi-Fi can reach the broker.
// Example: "192.168.1.10". Do NOT use "localhost" on firmware.
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883

// Pre-provisioned Device UUID token from Django Admin (/admin/monitor/device/)
#define DEVICE_AUTH_TOKEN "00000000-0000-0000-0000-000000000000"
