#pragma once

#include <Arduino.h>

enum class Zone : uint8_t {
  Normal = 0,
  Warning,
  Critical,
  SensorFault
};

inline const char *zoneName(Zone z) {
  switch (z) {
    case Zone::Normal:
      return "NORMAL";
    case Zone::Warning:
      return "WARNING";
    case Zone::Critical:
      return "CRITICAL";
    case Zone::SensorFault:
      return "FAULT";
    default:
      return "?";
  }
}

struct SensorSample {
  bool valid;
  float temperatureC;
  float humidityPct;
  uint32_t timestampMs;
  uint8_t failStreak;
  bool faultInjected;
};

struct SystemState {
  SensorSample sample;
  Zone zone;
  bool persistentAlarm;
  bool buzzerMuted;
  bool coolingOn;
  bool recovering;
  uint32_t excursionStartMs;
  uint32_t excursionMs;
  uint32_t zoneEnteredMs;
  uint32_t alarmLatchedMs;
  uint32_t recoveryStartMs;
  uint32_t lastValidMs;
  char lastEvent[48];
};

inline void setEvent(SystemState &s, const char *msg) {
  strncpy(s.lastEvent, msg, sizeof(s.lastEvent) - 1);
  s.lastEvent[sizeof(s.lastEvent) - 1] = '\0';
}
