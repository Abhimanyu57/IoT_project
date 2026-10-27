#include "zone_logic.h"

static Zone classifyRaw(float t, const Thresholds &th) {
  if (t <= th.critLowC || t >= th.critHighC) {
    return Zone::Critical;
  }
  if (t <= th.warnLowC || t >= th.warnHighC) {
    return Zone::Warning;
  }
  return Zone::Normal;
}

// Hysteresis: leaving a worse zone is harder than entering it.
static Zone applyHysteresis(Zone current, float t, const Thresholds &th) {
  const float h = th.hysteresisC;
  Zone raw = classifyRaw(t, th);

  if (current == Zone::SensorFault) {
    return raw;
  }

  if (current == Zone::Critical) {
    bool stillCrit = (t <= th.critLowC + h) || (t >= th.critHighC - h);
    if (stillCrit) {
      return Zone::Critical;
    }
    return raw; // warning or normal
  }

  if (current == Zone::Warning) {
    if (raw == Zone::Critical) {
      return Zone::Critical;
    }
    bool stillWarn = (t <= th.warnLowC + h) || (t >= th.warnHighC - h);
    if (stillWarn) {
      return Zone::Warning;
    }
    return Zone::Normal;
  }

  return raw;
}

static void enterZone(SystemState &state, Zone next, uint32_t nowMs,
                      const char *why) {
  if (state.zone != next) {
    state.zone = next;
    state.zoneEnteredMs = nowMs;
    setEvent(state, why);
  }
}

void zoneInit(SystemState &state) {
  memset(&state, 0, sizeof(state));
  state.zone = Zone::Normal;
  setEvent(state, "boot");
}

void zoneReset(SystemState &state, uint32_t nowMs) {
  state.persistentAlarm = false;
  state.buzzerMuted = false;
  state.recovering = false;
  state.excursionMs = 0;
  state.excursionStartMs = 0;
  state.alarmLatchedMs = 0;
  state.recoveryStartMs = 0;
  enterZone(state, Zone::Normal, nowMs, "manual-reset");
}

void zoneAcknowledge(SystemState &state, uint32_t nowMs) {
  state.buzzerMuted = true;
  setEvent(state, "ack-mute");
  (void)nowMs;
}

void zoneEvaluate(SystemState &state, const Thresholds &th, uint32_t nowMs) {
  const SensorSample &s = state.sample;

  if (!s.valid && s.failStreak >= th.sensorFailStreak) {
    if (state.zone != Zone::SensorFault) {
      enterZone(state, Zone::SensorFault, nowMs, "sensor-fail");
      state.persistentAlarm = true;
      state.buzzerMuted = false;
      state.alarmLatchedMs = nowMs;
      state.recovering = false;
    }
    if (state.excursionStartMs == 0) {
      state.excursionStartMs = nowMs;
    }
    state.excursionMs = nowMs - state.excursionStartMs;
    return;
  }

  if (!s.valid) {
    // Transient DHT miss: keep last zone, do not yet declare fault.
    return;
  }

  state.lastValidMs = nowMs;
  Zone next = applyHysteresis(state.zone, s.temperatureC, th);

  if (state.zone == Zone::SensorFault && next != Zone::SensorFault) {
    setEvent(state, "sensor-restored");
  }

  if (next != state.zone) {
    char buf[48];
    snprintf(buf, sizeof(buf), "zone %s -> %s", zoneName(state.zone),
             zoneName(next));
    enterZone(state, next, nowMs, buf);
    if (next != Zone::Normal) {
      state.buzzerMuted = false;
    }
  }

  if (state.zone == Zone::Normal) {
    if (state.persistentAlarm) {
      if (!state.recovering) {
        state.recovering = true;
        state.recoveryStartMs = nowMs;
        setEvent(state, "recovery-hold");
      } else if ((nowMs - state.recoveryStartMs) >= th.recoveryHoldMs) {
        state.persistentAlarm = false;
        state.recovering = false;
        state.buzzerMuted = false;
        state.excursionStartMs = 0;
        state.excursionMs = 0;
        state.alarmLatchedMs = 0;
        setEvent(state, "auto-recovered");
      }
    } else {
      state.recovering = false;
      state.excursionStartMs = 0;
      state.excursionMs = 0;
    }
    return;
  }

  // Out of band (warning or critical)
  state.recovering = false;
  if (state.excursionStartMs == 0) {
    state.excursionStartMs = nowMs;
  }
  state.excursionMs = nowMs - state.excursionStartMs;

  uint32_t dwell = nowMs - state.zoneEnteredMs;
  if (state.zone == Zone::Critical && dwell >= th.criticalLatchMs) {
    if (!state.persistentAlarm) {
      state.persistentAlarm = true;
      state.alarmLatchedMs = nowMs;
      state.buzzerMuted = false;
      setEvent(state, "latch-critical");
    }
  }
  if (state.zone == Zone::Warning && dwell >= th.warningLatchMs) {
    if (!state.persistentAlarm) {
      state.persistentAlarm = true;
      state.alarmLatchedMs = nowMs;
      state.buzzerMuted = false;
      setEvent(state, "latch-warning-duration");
    }
  }
}
