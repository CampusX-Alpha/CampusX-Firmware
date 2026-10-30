#ifndef SENSOR_HAL_H_
#define SENSOR_HAL_H_
// SensorHAL.h — STB-style Hardware Abstraction Layer for CampusX edge nodes.
// Define SENSOR_HAL_IMPLEMENTATION in main.cpp before including this file.

#include <Arduino.h>
#include <stdarg.h>

constexpr size_t MAX_METRICS_PER_SENSOR = 4;
constexpr size_t MAX_SENSORS = 6;
constexpr size_t MAX_ACTUATORS = 4;

struct MetricReading {
  const char* datastream_key;
  float value;
};

typedef size_t (*SensorReadFn)(uint8_t pin, MetricReading* out_readings,
                               size_t max_slots);

struct SensorConfig {
  const char* name;
  uint8_t pin;
  SensorReadFn read_func;
};

typedef void (*ActuatorControlFn)(uint8_t pin, float value);

struct ActuatorConfig {
  const char* command_key;
  uint8_t pin;
  ActuatorControlFn control_func;
};

// Safe payload & zero-allocation JSON helpers
int append_to_payload(char* buf, size_t buf_len, size_t used, const char* fmt, ...);
float extract_json_float(const char* json, const char* key);
bool extract_json_string(const char* json, const char* key, char* out, size_t out_len);

// Hardware initialization & driver callbacks
void hal_init_sensors();
size_t hal_read_dht11(uint8_t pin, MetricReading* out, size_t max_slots);
size_t hal_read_analog_gas(uint8_t pin, MetricReading* out, size_t max_slots);
size_t hal_read_vbat(uint8_t pin, MetricReading* out, size_t max_slots);
void hal_control_servo(uint8_t pin, float value);

#endif  // SENSOR_HAL_H_

#ifdef SENSOR_HAL_IMPLEMENTATION
#ifndef SENSOR_HAL_IMPL_GUARD_
#define SENSOR_HAL_IMPL_GUARD_

#include <DHT.h>

#ifndef HAL_DHT_PIN
#define HAL_DHT_PIN GPIO_NUM_15
#endif
#ifndef HAL_DHT_TYPE
#define HAL_DHT_TYPE DHT11
#endif

static DHT g_hal_dht(HAL_DHT_PIN, HAL_DHT_TYPE);
static bool g_servo_attached = false;

void hal_init_sensors() {
  g_hal_dht.begin();
}

static void hal_servo_attach(uint8_t pin) {
  if (g_servo_attached) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin, 50, 16);
#else
  ledcSetup(0, 50, 16);
  ledcAttachPin(pin, 0);
#endif
  g_servo_attached = true;
}

static void hal_servo_set_angle(uint8_t pin, int angle) {
  hal_servo_attach(pin);
  if (angle < 0) angle = 0;
  if (angle > 180) angle = 180;
  uint32_t duty = 3276 + (uint32_t)((6553 - 3276) * angle / 180);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin, duty);
#else
  ledcWrite(0, duty);
#endif
}

int append_to_payload(char* buf, size_t buf_len, size_t used, const char* fmt, ...) {
  if (used >= buf_len) return static_cast<int>(buf_len);
  size_t remaining = buf_len - used;
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buf + used, remaining, fmt, args);
  va_end(args);
  if (n < 0) return static_cast<int>(used);
  if (static_cast<size_t>(n) >= remaining) return static_cast<int>(buf_len - 1);
  return static_cast<int>(used) + n;
}

float extract_json_float(const char* json, const char* key) {
  char pattern[64];
  int plen = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* p = (plen > 0) ? strstr(json, pattern) : strstr(json, key);
  if (!p) return NAN;
  p = strchr(p, ':');
  if (!p) return NAN;
  ++p;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' || *p == '"') ++p;
  return static_cast<float>(atof(p));
}

bool extract_json_string(const char* json, const char* key, char* out, size_t out_len) {
  char pattern[64];
  int plen = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  const char* p = (plen > 0) ? strstr(json, pattern) : strstr(json, key);
  if (!p) return false;
  p = strchr(p, ':');
  if (!p) return false;
  ++p;
  while (*p == ' ' || *p == '\t') ++p;
  if (*p != '"') return false;
  ++p;
  size_t n = 0;
  while (*p && *p != '"' && n < out_len - 1) {
    out[n++] = *p++;
  }
  out[n] = '\0';
  return n > 0;
}

size_t hal_read_dht11(uint8_t pin, MetricReading* out, size_t max_slots) {
  (void)pin;
  if (max_slots < 2) return 0;
  float t = g_hal_dht.readTemperature();
  float h = g_hal_dht.readHumidity();
  if (isnan(t)) t = -127.0f;
  int hum_int = isnan(h) ? -1 : static_cast<int>(h);
  out[0] = {"temp", t};
  out[1] = {"hum", static_cast<float>(hum_int)};
  return 2;
}

size_t hal_read_analog_gas(uint8_t pin, MetricReading* out, size_t max_slots) {
  (void)pin;
  if (max_slots < 1) return 0;
  out[0] = {"gas", 420.0f};
  return 1;
}

size_t hal_read_vbat(uint8_t pin, MetricReading* out, size_t max_slots) {
  if (max_slots < 1) return 0;
  int raw = analogRead(pin);
  float vbat = static_cast<float>(raw) * 3.3f / 4095.0f * 2.0f;
  out[0] = {"vbat", vbat};
  return 1;
}

void hal_control_servo(uint8_t pin, float value) {
  hal_servo_set_angle(pin, static_cast<int>(value));
}

#endif  // SENSOR_HAL_IMPL_GUARD_
#endif  // SENSOR_HAL_IMPLEMENTATION