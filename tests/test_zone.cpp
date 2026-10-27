#include "cooling.h"
#include "zone_logic.h"

#include <cassert>
#include <cstdio>

static SensorSample validAt(float t, uint32_t now) {
  SensorSample s;
  memset(&s, 0, sizeof(s));
  s.valid = true;
  s.temperatureC = t;
  s.humidityPct = 60;
  s.timestampMs = now;
  return s;
}

static SensorSample failAt(uint8_t streak, uint32_t now, bool injected) {
  SensorSample s;
  memset(&s, 0, sizeof(s));
  s.valid = false;
  s.failStreak = streak;
  s.faultInjected = injected;
  s.timestampMs = now;
  return s;
}

static void step(SystemState &st, Thresholds &th, float t, uint32_t now) {
  st.sample = validAt(t, now);
  zoneEvaluate(st, th, now);
  coolingUpdate(st, th);
}

int main() {
  Thresholds th = defaultThresholds();
  // Speed up duration tests (logic uses the same fields).
  th.warningLatchMs = 15000;
  th.criticalLatchMs = 3000;
  th.recoveryHoldMs = 4000;

  SystemState st;
  zoneInit(st);

  // 1. Normal at 5 °C, cooling off (below coolOn 6).
  step(st, th, 5.0f, 1000);
  assert(st.zone == Zone::Normal);
  assert(!st.persistentAlarm);
  assert(!st.coolingOn);
  std::puts("PASS normal 5C");

  // 2. Warning high at 8.5 °C, cooling on.
  step(st, th, 8.5f, 2000);
  assert(st.zone == Zone::Warning);
  assert(!st.persistentAlarm);
  assert(st.coolingOn);
  assert(st.excursionMs == 0 || st.excursionStartMs == 2000);
  std::puts("PASS warning 8.5C + cooling on");

  // 3. Hysteresis: 7.6 °C still warning (8.0 - 0.5 = 7.5).
  step(st, th, 7.6f, 3000);
  assert(st.zone == Zone::Warning);
  std::puts("PASS hysteresis hold warning at 7.6C");

  // 4. 7.4 °C returns to normal (no latch yet — dwell was short).
  step(st, th, 7.4f, 4000);
  assert(st.zone == Zone::Normal);
  assert(!st.persistentAlarm);
  std::puts("PASS hysteresis release to normal at 7.4C");

  // 5. Prolonged warning latches after 15 s dwell.
  step(st, th, 8.5f, 10000);
  assert(st.zone == Zone::Warning);
  step(st, th, 8.5f, 10000 + 14999);
  assert(!st.persistentAlarm);
  step(st, th, 8.5f, 10000 + 15000);
  assert(st.persistentAlarm);
  std::puts("PASS warning duration latch at 15s");

  // 6. ACK mutes but keeps latch.
  zoneAcknowledge(st, 26000);
  assert(st.buzzerMuted);
  assert(st.persistentAlarm);
  std::puts("PASS ack mutes buzzer, latch remains");

  // 7. Critical at 11 °C.
  step(st, th, 11.0f, 30000);
  assert(st.zone == Zone::Critical);
  step(st, th, 11.0f, 33000);
  assert(st.persistentAlarm);
  std::puts("PASS critical 11C");

  // 8. Recovery: back in band must wait recoveryHoldMs.
  step(st, th, 5.0f, 40000);
  assert(st.zone == Zone::Normal);
  assert(st.recovering);
  assert(st.persistentAlarm);
  step(st, th, 5.0f, 40000 + 3999);
  assert(st.persistentAlarm);
  step(st, th, 5.0f, 40000 + 4000);
  assert(!st.persistentAlarm);
  assert(!st.recovering);
  assert(st.excursionMs == 0);
  std::puts("PASS auto-recovery after 4s in-band");

  // 9. Sensor fault after streak.
  zoneInit(st);
  st.sample = failAt(2, 1000, true);
  zoneEvaluate(st, th, 1000);
  assert(st.zone != Zone::SensorFault);
  st.sample = failAt(3, 2000, true);
  zoneEvaluate(st, th, 2000);
  coolingUpdate(st, th);
  assert(st.zone == Zone::SensorFault);
  assert(st.persistentAlarm);
  assert(st.coolingOn);
  std::puts("PASS sensor fault streak + failsafe cooling");

  // 10. Restore sensor then recover.
  step(st, th, 5.0f, 3000);
  assert(st.zone == Zone::Normal);
  step(st, th, 5.0f, 3000 + 4000);
  assert(!st.persistentAlarm);
  std::puts("PASS recover after sensor restore");

  // 11. Cooling hysteresis: ON at 6, stays on until 4.
  zoneInit(st);
  step(st, th, 5.9f, 100);
  assert(!st.coolingOn);
  step(st, th, 6.0f, 200);
  assert(st.coolingOn);
  step(st, th, 4.1f, 300);
  assert(st.coolingOn);
  step(st, th, 4.0f, 400);
  assert(!st.coolingOn);
  std::puts("PASS cooling relay hysteresis 6/4");

  std::puts("ALL ZONE/COOLING TESTS PASSED");
  return 0;
}
