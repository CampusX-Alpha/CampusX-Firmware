#include <Arduino.h>
#include <ArduinoJson.h>

// Bring in STB implementations
#define EDGE_NODE_IMPLEMENTATION
#include "EdgeNode.h"

#define SENSOR_HAL_IMPLEMENTATION
#include "SensorHAL.h"

// 1. Configuration (Keep out of JSON payload)
const char* WIFI_SSID     = "SECRET_SSID";
const char* WIFI_PASS     = "SECRET_PASS";
const char* MQTT_BROKER   = "192.168.1.100"; // Replace with your Mosquitto IP
const uint16_t MQTT_PORT  = 1883;
const char* AUTH_TOKEN    = "campusx-auth-token"; // EdgeNode requires this constructor arg
const char* DEVICE_ID     = "esp32-lab-01";       // Must match active CampusX database DeviceCode

class CampusXNode : public EdgeNode {
private:
    unsigned long last_msg_time = 0;
    uint32_t msg_counter = 0;

public:
    CampusXNode() : EdgeNode(WIFI_SSID, WIFI_PASS, MQTT_BROKER, MQTT_PORT, AUTH_TOKEN) {}

    void node_setup() override {
        hal_init_sensors();
    }

    void node_task() override {
        if (!_mqtt.connected()) {
            reconnectMQTT();
        }
        _mqtt.loop();

        unsigned long now = millis();
        if (now - last_msg_time > 5000) {
            last_msg_time = now;
            publish_telemetry();
        }
    }

    void publish_telemetry() {
        // Stack-allocated array for pointer-based HAL reading
        MetricReading readings[2];
        size_t count = hal_read_dht11(HAL_DHT_PIN, readings, 2);
        
        if (count < 2) return; // Ensure we have both temp and hum

        // ArduinoJson 7 allocates entirely on the stack/custom pool. No heap fragmentation.
        JsonDocument doc;
        
        // Root payload fields
        doc["device_id"] = DEVICE_ID;
        
        // Generate strict unique message_id: "esp32-lab-01-000001"
        char msg_id[48];
        snprintf(msg_id, sizeof(msg_id), "%s-%06lu", DEVICE_ID, ++msg_counter);
        doc["message_id"] = msg_id;

        // Construct readings array
        JsonArray json_readings = doc["readings"].to<JsonArray>();

        // Temperature Object (readings[0] mapped by the HAL)
        JsonObject temperature = json_readings.add<JsonObject>();
        temperature["metric"] = "temperature"; 
        temperature["value"] = readings[0].value; 
        temperature["unit"] = "C";

        // Humidity Object (readings[1] mapped by the HAL)
        JsonObject humidity = json_readings.add<JsonObject>();
        humidity["metric"] = "humidity";
        humidity["value"] = readings[1].value;
        humidity["unit"] = "%";

        // Serialize to a static C-string buffer
        char payload[512];
        size_t length = serializeJson(doc, payload, sizeof(payload));

        if (length > 0 && length < sizeof(payload)) {
            // Publish directly to the exact telemetry topic required by the backend
            _mqtt.publish("smart-campus/telemetry", payload, false);
        }
    }
};

// Global node instance
CampusXNode node;

void setup() {
    Serial.begin(115200);
    node.begin(); 
}

void loop() {
    node.run();
}