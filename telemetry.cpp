#include "telemetry.h"

#if CCG_ENABLE_MQTT

#include <PubSubClient.h>
#include <WiFi.h>

#include "zone_logic.h"

static WiFiClient gNet;
static PubSubClient gMqtt(gNet);
static uint32_t gLastWifiAttempt = 0;
static uint32_t gLastMqttAttempt = 0;
static uint32_t gLastPub = 0;
static bool gWifiUp = false;
static bool gMqttUp = false;
static Thresholds *gTh = nullptr;
static SystemState *gState = nullptr;

static void onMqtt(char *topic, byte *payload, unsigned int len) {
  char buf[96];
  unsigned int n = (len < sizeof(buf) - 1) ? len : (sizeof(buf) - 1);
  memcpy(buf, payload, n);
  buf[n] = '\0';
  Serial.printf("[mqtt] %s : %s\n", topic, buf);

  if (!gState || !gTh) {
    return;
  }
  if (strcmp(buf, "ack") == 0) {
    zoneAcknowledge(*gState, millis());
    return;
  }
  if (strcmp(buf, "reset") == 0) {
    zoneReset(*gState, millis());
    return;
  }
  if (strncmp(buf, "set ", 4) == 0) {
    // Reuse serial parser path by printing a hint; keep MQTT command simple.
    float v = 0;
    char key[24];
    if (sscanf(buf + 4, "%23s %f", key, &v) == 2) {
      if (strcmp(key, "warn_high") == 0 && v > 0 && v < 40) {
        gTh->warnHighC = v;
      } else if (strcmp(key, "warn_low") == 0 && v > -20 && v < 20) {
        gTh->warnLowC = v;
      }
      setEvent(*gState, "mqtt-set");
    }
  }
}

static void ensureWifi(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    gWifiUp = true;
    return;
  }
  gWifiUp = false;
  gMqttUp = false;
  if (gLastWifiAttempt != 0 && (nowMs - gLastWifiAttempt) < kWifiRetryMs) {
    return;
  }
  gLastWifiAttempt = nowMs;
  Serial.println(F("[wifi] connecting to Wokwi-GUEST"));
  WiFi.mode(WIFI_STA);
  WiFi.begin(kWifiSsid, kWifiPass, kWifiChannel);
}

static void ensureMqtt(uint32_t nowMs) {
  if (!gWifiUp) {
    return;
  }
  if (gMqtt.connected()) {
    gMqttUp = true;
    gMqtt.loop();
    return;
  }
  gMqttUp = false;
  if (gLastMqttAttempt != 0 && (nowMs - gLastMqttAttempt) < kMqttRetryMs) {
    return;
  }
  gLastMqttAttempt = nowMs;
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "%s%lu", kMqttClientPrefix,
           (unsigned long)(nowMs & 0xFFFF));
  Serial.printf("[mqtt] connect %s as %s\n", kMqttHost, clientId);
  if (gMqtt.connect(clientId)) {
    gMqttUp = true;
    gMqtt.subscribe(kMqttSubTopic);
    Serial.println(F("[mqtt] up"));
  } else {
    Serial.printf("[mqtt] failed rc=%d (control loop still runs)\n",
                  gMqtt.state());
  }
}

static void publish(const SystemState &state, const Thresholds &th,
                    uint32_t nowMs) {
  if (!gMqttUp) {
    return;
  }
  if (gLastPub != 0 && (nowMs - gLastPub) < kTelemetryPeriodMs) {
    return;
  }
  gLastPub = nowMs;

  char json[256];
  snprintf(json, sizeof(json),
           "{\"sim\":true,\"id\":\"%s\",\"t\":%.2f,\"h\":%.1f,\"valid\":%s,"
           "\"zone\":\"%s\",\"alarm\":%s,\"cool\":%s,\"exc_ms\":%lu,"
           "\"fault\":%s,\"mute\":%s,\"warn\":[%.1f,%.1f]}",
           kDeviceId, state.sample.valid ? state.sample.temperatureC : -99.0f,
           state.sample.valid ? state.sample.humidityPct : -1.0f,
           state.sample.valid ? "true" : "false", zoneName(state.zone),
           state.persistentAlarm ? "true" : "false",
           state.coolingOn ? "true" : "false",
           (unsigned long)state.excursionMs,
           state.sample.faultInjected ? "true" : "false",
           state.buzzerMuted ? "true" : "false", th.warnLowC, th.warnHighC);
  gMqtt.publish(kMqttPubTopic, json, false);
}

void telemetryInit() {
  gMqtt.setServer(kMqttHost, kMqttPort);
  gMqtt.setCallback(onMqtt);
  gMqtt.setBufferSize(384);
  Serial.println(F("[mqtt] optional; core control does not wait for network"));
}

void telemetryUpdate(SystemState &state, Thresholds &th, uint32_t nowMs) {
  gState = &state;
  gTh = &th;
  ensureWifi(nowMs);
  if (WiFi.status() == WL_CONNECTED && !gWifiUp) {
    gWifiUp = true;
    Serial.print(F("[wifi] ip "));
    Serial.println(WiFi.localIP());
  }
  ensureMqtt(nowMs);
  publish(state, th, nowMs);
}

bool telemetryWifiUp() { return gWifiUp; }
bool telemetryMqttUp() { return gMqttUp; }

#else

void telemetryInit() {}
void telemetryUpdate(SystemState &, Thresholds &, uint32_t) {}
bool telemetryWifiUp() { return false; }
bool telemetryMqttUp() { return false; }

#endif
