#ifndef EDGE_NODE_H_
#define EDGE_NODE_H_
// EdgeNode.h — STB-style Base Transport & NVS Backlog Layer for CampusX nodes.
// Define EDGE_NODE_IMPLEMENTATION in main.cpp before including this file.

#include <WiFi.h>
#include <PubSubClient.h>

extern int g_bootCount;
extern int g_backlogHead;
extern int g_backlogCount;

class EdgeNode {
 public:
  EdgeNode(const char* ssid, const char* pass, const char* broker,
           uint16_t port, const char* authToken);
  virtual ~EdgeNode() = default;

  void begin();
  void run();

  virtual void node_setup() = 0;
  virtual void node_task() = 0;
  virtual void sync_offline_backlog();

 protected:
  bool connectWiFi(uint32_t timeoutMs = 10000);
  bool reconnectMQTT(uint32_t timeoutMs = 5000);
  void buildTopic(const char* leaf, char* out, size_t outLen) const;
  void stashOffline(const char* payload, int maxBacklog = 10);

  const char* _ssid;
  const char* _pass;
  const char* _broker;
  uint16_t _port;
  const char* _authToken;

  WiFiClient _wifi;
  PubSubClient _mqtt;
};

#endif  // EDGE_NODE_H_

#ifdef EDGE_NODE_IMPLEMENTATION
#ifndef EDGE_NODE_IMPL_GUARD_
#define EDGE_NODE_IMPL_GUARD_

#include <Preferences.h>

RTC_DATA_ATTR int g_bootCount = 0;
RTC_DATA_ATTR int g_backlogHead = 0;
RTC_DATA_ATTR int g_backlogCount = 0;

EdgeNode::EdgeNode(const char* ssid, const char* pass, const char* broker,
                   uint16_t port, const char* authToken)
    : _ssid(ssid),
      _pass(pass),
      _broker(broker),
      _port(port),
      _authToken(authToken),
      _mqtt(_wifi) {}

void EdgeNode::begin() {
  g_bootCount++;
  connectWiFi(10000);
  _mqtt.setServer(_broker, _port);
  node_setup();
}

void EdgeNode::run() {
  node_task();
}

bool EdgeNode::connectWiFi(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  WiFi.mode(WIFI_STA);
  WiFi.begin(_ssid, _pass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeoutMs) return false;
    delay(250);
  }
  return true;
}

bool EdgeNode::reconnectMQTT(uint32_t timeoutMs) {
  if (_mqtt.connected()) return true;
  uint32_t start = millis();
  while (!_mqtt.connected()) {
    char clientId[32];
    snprintf(clientId, sizeof(clientId), "edge-%08X", (unsigned)ESP.getEfuseMac());
    if (_mqtt.connect(clientId)) return true;
    if (millis() - start > timeoutMs) return false;
    delay(500);
  }
  return false;
}

void EdgeNode::buildTopic(const char* leaf, char* out, size_t outLen) const {
  snprintf(out, outLen, "campusx/devices/%s/%s", _authToken, leaf);
}

void EdgeNode::stashOffline(const char* payload, int maxBacklog) {
  Preferences prefs;
  if (!prefs.begin("backlog", false)) return;
  int count = prefs.getInt("count", 0);
  if (count >= maxBacklog) {
    prefs.end();
    return;
  }
  char key[8];
  snprintf(key, sizeof(key), "msg%d", count);
  prefs.putString(key, payload);
  prefs.putInt("count", count + 1);
  prefs.end();
  g_backlogCount = count + 1;
}

void EdgeNode::sync_offline_backlog() {
  if (!_mqtt.connected()) return;
  Preferences prefs;
  if (!prefs.begin("backlog", false)) return;
  int count = prefs.getInt("count", 0);
  if (count <= 0) {
    prefs.end();
    return;
  }
  char topic[96];
  buildTopic("data", topic, sizeof(topic));
  char key[8];
  char msg[256];
  bool all_ok = true;
  
  for (int i = 0; i < count && i < 10; i++) {
    yield();
    snprintf(key, sizeof(key), "msg%d", i);
    
    // Memory-Safe Fix: Read directly into the static 'msg' char buffer.
    // This entirely bypasses the heap-fragmenting Arduino String allocation.
    size_t len = prefs.getString(key, msg, sizeof(msg));
    if (len == 0) continue;
    
    if (!_mqtt.publish(topic, msg)) {
      all_ok = false;
      break;
    }
    _mqtt.loop();
  }
  if (all_ok) {
    prefs.putInt("count", 0);
    g_backlogCount = 0;
    g_backlogHead = 0;
  }
  prefs.end();
}

#endif  // EDGE_NODE_IMPL_GUARD_
#endif  // EDGE_NODE_IMPLEMENTATION