#include <Arduino.h>
#include <ArduinoJson.h>

// STB implementations (using your memory-safe NVS string fix)
#define EDGE_NODE_IMPLEMENTATION
#include "EdgeNode.h"

#define SENSOR_HAL_IMPLEMENTATION
#include "SensorHAL.h"

const char* WIFI_SSID     = "SECRET_SSID";
const char* WIFI_PASS     = "SECRET_PASS";
const char* MQTT_BROKER   = "192.168.1.100";
const uint16_t MQTT_PORT  = 1883;
const char* AUTH_TOKEN    = "campusx-auth-token";
const char* DEVICE_ID     = "esp32-lab-01";

// Deep sleep timing constants
const uint64_t SLEEP_MINUTES = 10;
const uint64_t MICROSECONDS_PER_MINUTE = 60000000ULL;

class CampusXNode : public EdgeNode {
public:
    CampusXNode() : EdgeNode(WIFI_SSID, WIFI_PASS, MQTT_BROKER, MQTT_PORT, AUTH_TOKEN) {}

    // node_setup() now contains the entire lifespan of the active processor
    void node_setup() override {
        hal_init_sensors();

        // EdgeNode::begin() already connected WiFi, so we just connect MQTT
        if (reconnectMQTT()) {
            publish_telemetry();
            
            // Allow the TCP stack time to flush the outbound packet to the modem
            _mqtt.loop();
            delay(50);
        }

        // Hand control over to the RTC coprocessor watchdog and cut main power
        esp_sleep_enable_timer_wakeup(SLEEP_MINUTES * MICROSECONDS_PER_MINUTE);
        esp_deep_sleep_start();
    }

    // The pure virtual function must exist, but execution will never reach it
    void node_task() override {}

private:
    void publish_telemetry() {
        MetricReading readings[2];
        size_t count = hal_read_dht11(HAL_DHT_PIN, readings, 2);
        
        if (count < 2) return;

        JsonDocument doc;
        doc["device_id"] = DEVICE_ID;
        
        // Utilize the RTC-backed g_bootCount to guarantee a unique ID across power cycles
        char msg_id[48];
        snprintf(msg_id, sizeof(msg_id), "%s-%06d", DEVICE_ID, g_bootCount);
        doc["message_id"] = msg_id;

        JsonArray json_readings = doc["readings"].to<JsonArray>();

        JsonObject temperature = json_readings.add<JsonObject>();
        temperature["metric"] = "temperature"; 
        temperature["value"] = readings[0].value; 
        temperature["unit"] = "C";

        JsonObject humidity = json_readings.add<JsonObject>();
        humidity["metric"] = "humidity";
        humidity["value"] = readings[1].value;
        humidity["unit"] = "%";

        char payload[512];
        size_t length = serializeJson(doc, payload, sizeof(payload));

        if (length > 0 && length < sizeof(payload)) {
            _mqtt.publish("smart-campus/telemetry", payload, false);
        }
    }
};

CampusXNode node;

void setup() {
    Serial.begin(115200);
    // begin() runs connectWiFi(), calls node_setup(), and the chip dies before returning
    node.begin(); 
}

void loop() {
    // The standard Arduino loop is now completely dead code
}