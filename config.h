#pragma once

#include <Arduino.h>

// ColdChain Guardian — SIMULATION ONLY
// Not a tested physical refrigeration or medical-device controller.

static const char kFirmwareVersion[] = "1.0.0-sim";
static const char kDeviceId[] = "ccg-esp32-01";

// --- Pins (ESP32 DevKit C v4 / Wokwi) ------------------------------------
static const int kPinDht = 4;
static const int kPinOledSda = 21;
static const int kPinOledScl = 22;
static const int kPinLedGreen = 25;
static const int kPinLedYellow = 26;
static const int kPinLedRed = 27;
static const int kPinBuzzer = 14;
static const int kPinRelay = 13;
static const int kPinAckButton = 32;   // active-low, internal pull-up
static const int kPinFaultSwitch = 33; // LOW = injected sensor failure

static const uint8_t kOledI2cAddr = 0x3C;
static const int kOledWidth = 128;
static const int kOledHeight = 64;

// --- Timing --------------------------------------------------------------
static const uint32_t kSensorPeriodMs = 1000;
static const uint32_t kDisplayPeriodMs = 250;
static const uint32_t kActuatorPeriodMs = 40;
static const uint32_t kTelemetryPeriodMs = 2000;
static const uint32_t kWifiRetryMs = 4000;
static const uint32_t kMqttRetryMs = 4000;
static const uint32_t kDebounceMs = 40;

// --- Default cold-chain band (vaccine-style 2–8 °C, simulation) ---------
struct Thresholds {
  float warnLowC;
  float warnHighC;
  float critLowC;
  float critHighC;
  float hysteresisC;
  float coolOnC;
  float coolOffC;
  uint32_t warningLatchMs;
  uint32_t criticalLatchMs;
  uint32_t recoveryHoldMs;
  uint8_t sensorFailStreak;
};

inline Thresholds defaultThresholds() {
  Thresholds t;
  t.warnLowC = 2.0f;
  t.warnHighC = 8.0f;
  t.critLowC = 0.0f;
  t.critHighC = 10.0f;
  t.hysteresisC = 0.5f;
  t.coolOnC = 6.0f;
  t.coolOffC = 4.0f;
  t.warningLatchMs = 15000;   // prolonged warning -> persistent alarm
  t.criticalLatchMs = 3000;   // short critical dwell still latches
  t.recoveryHoldMs = 4000;    // must stay in-band this long to auto-clear
  t.sensorFailStreak = 3;
  return t;
}

// --- Optional Wi-Fi / MQTT (does not gate cooling or alarms) ------------
#ifndef CCG_ENABLE_MQTT
#define CCG_ENABLE_MQTT 1
#endif

static const char kWifiSsid[] = "Wokwi-GUEST";
static const char kWifiPass[] = "";
static const int kWifiChannel = 6;

static const char kMqttHost[] = "broker.hivemq.com";
static const uint16_t kMqttPort = 1883;
static const char kMqttPubTopic[] = "coldchain/guardian/telemetry";
static const char kMqttSubTopic[] = "coldchain/guardian/cmd";
static const char kMqttClientPrefix[] = "ccg-";
