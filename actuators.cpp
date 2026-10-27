#include "actuators.h"

#include "config.h"

static uint32_t gLastActMs = 0;
static bool gToneOn = false;

void actuatorsInit() {
  pinMode(kPinLedGreen, OUTPUT);
  pinMode(kPinLedYellow, OUTPUT);
  pinMode(kPinLedRed, OUTPUT);
  pinMode(kPinRelay, OUTPUT);
  pinMode(kPinBuzzer, OUTPUT);
  digitalWrite(kPinLedGreen, LOW);
  digitalWrite(kPinLedYellow, LOW);
  digitalWrite(kPinLedRed, LOW);
  digitalWrite(kPinRelay, LOW);
  noTone(kPinBuzzer);
}

void actuatorsSilenceTone() {
  noTone(kPinBuzzer);
  gToneOn = false;
}

static void setLeds(bool g, bool y, bool r) {
  digitalWrite(kPinLedGreen, g ? HIGH : LOW);
  digitalWrite(kPinLedYellow, y ? HIGH : LOW);
  digitalWrite(kPinLedRed, r ? HIGH : LOW);
}

void actuatorsUpdate(const SystemState &state, uint32_t nowMs) {
  if (gLastActMs != 0 && (nowMs - gLastActMs) < kActuatorPeriodMs) {
    return;
  }
  gLastActMs = nowMs;

  digitalWrite(kPinRelay, state.coolingOn ? HIGH : LOW);

  const uint32_t phase = nowMs / 200;
  const bool blink = (phase % 2) == 0;

  switch (state.zone) {
    case Zone::Normal:
      if (state.persistentAlarm || state.recovering) {
        setLeds(blink, false, false); // green blink during recovery hold
      } else {
        setLeds(true, false, false);
      }
      break;
    case Zone::Warning:
      setLeds(false, true, false);
      break;
    case Zone::Critical:
      setLeds(false, blink, true);
      break;
    case Zone::SensorFault:
      setLeds(false, false, blink);
      break;
  }

  if (state.buzzerMuted ||
      (state.zone == Zone::Normal && !state.persistentAlarm)) {
    actuatorsSilenceTone();
    return;
  }

  // Persistent alarm: faster / higher tone. Soft warning: slow beep.
  bool wantTone = false;
  int freq = 880;
  if (state.zone == Zone::SensorFault) {
    wantTone = ((nowMs / 150) % 2) == 0;
    freq = 1500;
  } else if (state.zone == Zone::Critical || state.persistentAlarm) {
    wantTone = ((nowMs / 250) % 2) == 0;
    freq = 1200;
  } else if (state.zone == Zone::Warning) {
    wantTone = ((nowMs / 700) % 2) == 0;
    freq = 740;
  }

  if (wantTone && !gToneOn) {
    tone(kPinBuzzer, freq);
    gToneOn = true;
  } else if (!wantTone && gToneOn) {
    noTone(kPinBuzzer);
    gToneOn = false;
  }
}
