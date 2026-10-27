/*
 * ColdChain Guardian — ESP32 firmware for Wokwi simulation.
 *
 * SIMULATION ONLY. This is not a certified or physically tested
 * refrigeration / medical cold-chain controller.
 *
 * Core loop (always runs, even if Wi-Fi/MQTT fail):
 *   read DHT22 -> classify zone with hysteresis -> latch alarms ->
 *   drive relay/LEDs/buzzer/OLED.
 *
 * Optional: Wokwi-GUEST Wi-Fi + MQTT telemetry (telemetry.cpp).
 */

#include "actuators.h"
#include "config.h"
#include "cooling.h"
#include "inputs.h"
#include "sensor_reader.h"
#include "serial_cli.h"
#include "telemetry.h"
#include "types.h"
#include "ui_display.h"
#include "zone_logic.h"

static Thresholds gTh;
static SystemState gState;

void setup() {
  Serial.begin(115200);
  delay(200);

  gTh = defaultThresholds();
  zoneInit(gState);

  actuatorsInit();
  inputsInit();
  displayInit();
  displaySplash();
  sensorInit();
  cliInit();
  telemetryInit();

  delay(1200);
  Serial.println(F("loop start — click DHT22 in Wokwi to change temperature"));
}

void loop() {
  const uint32_t now = millis();

  inputsPoll(now, gState, gTh);
  sensorForceFail(inputsFaultInjected());
  sensorUpdate(now, gState.sample);
  zoneEvaluate(gState, gTh, now);
  coolingUpdate(gState, gTh);
  actuatorsUpdate(gState, now);
  displayUpdate(gState, gTh, telemetryWifiUp(), telemetryMqttUp(), now);
  telemetryUpdate(gState, gTh, now);
  cliPoll(gState, gTh);

  static uint32_t lastStatusMs = 0;
  if (lastStatusMs == 0 || (now - lastStatusMs) >= 1000) {
    lastStatusMs = now;
    Serial.printf(
        "STATUS zone=%s T=%.2f valid=%d alarm=%d cool=%d fault=%d mute=%d "
        "rcv=%d exc_ms=%lu event=%s\n",
        zoneName(gState.zone),
        gState.sample.valid ? gState.sample.temperatureC : -99.0f,
        gState.sample.valid, gState.persistentAlarm, gState.coolingOn,
        gState.sample.faultInjected, gState.buzzerMuted, gState.recovering,
        (unsigned long)gState.excursionMs, gState.lastEvent);
  }
}
