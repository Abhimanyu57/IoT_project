#include "cooling.h"

void coolingUpdate(SystemState &state, const Thresholds &th) {
  // Fail-safe: sensor fault keeps the simulated compressor engaged.
  if (state.zone == Zone::SensorFault) {
    state.coolingOn = true;
    return;
  }

  if (!state.sample.valid) {
    return;
  }

  const float t = state.sample.temperatureC;
  if (state.coolingOn) {
    if (t <= th.coolOffC) {
      state.coolingOn = false;
    }
  } else {
    if (t >= th.coolOnC) {
      state.coolingOn = true;
    }
  }
}
