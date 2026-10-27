#include "sensor_reader.h"

#include <DHTesp.h>

static DHTesp gDht;
static bool gInjectFail = false;
static uint8_t gFailStreak = 0;
static uint32_t gLastReadMs = 0;
static SensorSample gLast;

void sensorInit() {
  gDht.setup(kPinDht, DHTesp::DHT22);
  memset(&gLast, 0, sizeof(gLast));
  gLast.valid = false;
}

void sensorForceFail(bool inject) { gInjectFail = inject; }

bool sensorFaultInjected() { return gInjectFail; }

void sensorUpdate(uint32_t nowMs, SensorSample &out) {
  if (gLastReadMs != 0 && (nowMs - gLastReadMs) < kSensorPeriodMs) {
    out = gLast;
    out.faultInjected = gInjectFail;
    return;
  }
  gLastReadMs = nowMs;

  SensorSample s;
  memset(&s, 0, sizeof(s));
  s.timestampMs = nowMs;
  s.faultInjected = gInjectFail;

  if (gInjectFail) {
    gFailStreak = (gFailStreak < 255) ? (uint8_t)(gFailStreak + 1) : 255;
    s.valid = false;
    s.failStreak = gFailStreak;
    gLast = s;
    out = s;
    return;
  }

  TempAndHumidity v = gDht.getTempAndHumidity();
  bool ok = (gDht.getStatus() == DHTesp::ERROR_NONE) &&
            !isnan(v.temperature) && !isnan(v.humidity) &&
            v.temperature > -40.0f && v.temperature < 80.0f &&
            v.humidity >= 0.0f && v.humidity <= 100.0f;

  if (!ok) {
    gFailStreak = (gFailStreak < 255) ? (uint8_t)(gFailStreak + 1) : 255;
    s.valid = false;
    s.failStreak = gFailStreak;
    s.temperatureC = gLast.temperatureC;
    s.humidityPct = gLast.humidityPct;
    gLast = s;
    out = s;
    return;
  }

  gFailStreak = 0;
  s.valid = true;
  s.temperatureC = v.temperature;
  s.humidityPct = v.humidity;
  s.failStreak = 0;
  gLast = s;
  out = s;
}
