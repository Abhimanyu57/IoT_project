#include "inputs.h"

#include "config.h"
#include "zone_logic.h"

struct Debounced {
  int pin;
  bool lastRaw;
  bool stable;
  uint32_t lastChangeMs;
};

static Debounced gAck;
static Debounced gFault;
static bool gAckWasReleased = true;

static void initDebounced(Debounced &d, int pin) {
  d.pin = pin;
  pinMode(pin, INPUT_PULLUP);
  d.lastRaw = digitalRead(pin) == HIGH;
  d.stable = d.lastRaw;
  d.lastChangeMs = 0;
}

static bool updateDebounced(Debounced &d, uint32_t nowMs) {
  bool raw = digitalRead(d.pin) == HIGH;
  if (raw != d.lastRaw) {
    d.lastRaw = raw;
    d.lastChangeMs = nowMs;
  }
  if ((nowMs - d.lastChangeMs) >= kDebounceMs && d.stable != d.lastRaw) {
    d.stable = d.lastRaw;
    return true;
  }
  return false;
}

void inputsInit() {
  initDebounced(gAck, kPinAckButton);
  initDebounced(gFault, kPinFaultSwitch);
}

bool inputsFaultInjected() {
  // Switch LEFT shorts pin 2 to GND -> LOW on GPIO -> injected failure.
  return !gFault.stable;
}

void inputsPoll(uint32_t nowMs, SystemState &state, Thresholds &th) {
  (void)th;
  updateDebounced(gAck, nowMs);
  updateDebounced(gFault, nowMs);

  bool ackPressed = !gAck.stable; // active low
  if (ackPressed && gAckWasReleased) {
    zoneAcknowledge(state, nowMs);
    gAckWasReleased = false;
  }
  if (!ackPressed) {
    gAckWasReleased = true;
  }
}
