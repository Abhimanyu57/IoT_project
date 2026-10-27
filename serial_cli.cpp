#include "serial_cli.h"

#include "zone_logic.h"

static char gLine[96];
static uint8_t gLen = 0;

void cliInit() {
  Serial.println();
  Serial.println(F("=== ColdChain Guardian (SIMULATION) ==="));
  Serial.println(F("Not a tested physical refrigeration controller."));
  Serial.println(F("Commands: help status ack reset"));
  Serial.println(F("          set warn_low|warn_high|crit_low|crit_high <C>"));
  Serial.println(F("          set hyst|cool_on|cool_off <C>"));
  Serial.println(F("          set warn_latch_ms|crit_latch_ms|recovery_ms <n>"));
}

void cliPrintStatus(const SystemState &state, const Thresholds &th) {
  Serial.println(F("--- status ---"));
  Serial.printf("zone=%s alarm=%d mute=%d recovering=%d cool=%d\n",
                zoneName(state.zone), state.persistentAlarm, state.buzzerMuted,
                state.recovering, state.coolingOn);
  if (state.sample.valid) {
    Serial.printf("T=%.2fC H=%.1f%%\n", state.sample.temperatureC,
                  state.sample.humidityPct);
  } else {
    Serial.printf("T=INVALID failStreak=%u injected=%d\n",
                  state.sample.failStreak, state.sample.faultInjected);
  }
  Serial.printf("excursion_ms=%lu last_event=%s\n",
                (unsigned long)state.excursionMs, state.lastEvent);
  Serial.printf("band warn[%.1f,%.1f] crit[%.1f,%.1f] hyst=%.2f\n", th.warnLowC,
                th.warnHighC, th.critLowC, th.critHighC, th.hysteresisC);
  Serial.printf("cool on>=%.1f off<=%.1f latch warn=%lums crit=%lums rec=%lums\n",
                th.coolOnC, th.coolOffC, (unsigned long)th.warningLatchMs,
                (unsigned long)th.criticalLatchMs,
                (unsigned long)th.recoveryHoldMs);
}

static void printHelp() {
  Serial.println(F("help                 this text"));
  Serial.println(F("status               dump live state"));
  Serial.println(F("ack                  mute buzzer (alarm latch remains)"));
  Serial.println(F("reset                clear latch + excursion timers"));
  Serial.println(F("set <key> <value>    change a threshold"));
}

static bool setFloat(const char *key, const char *want, float &dst, float v,
                     float lo, float hi) {
  if (strcmp(key, want) != 0) {
    return false;
  }
  if (v < lo || v > hi) {
    Serial.println(F("value out of range"));
    return true;
  }
  dst = v;
  Serial.printf("%s = %.2f\n", want, dst);
  return true;
}

static bool setU32(const char *key, const char *want, uint32_t &dst, long v,
                   long lo, long hi) {
  if (strcmp(key, want) != 0) {
    return false;
  }
  if (v < lo || v > hi) {
    Serial.println(F("value out of range"));
    return true;
  }
  dst = (uint32_t)v;
  Serial.printf("%s = %lu\n", want, (unsigned long)dst);
  return true;
}

static void handleSet(Thresholds &th, char *args) {
  char key[24];
  char val[24];
  if (sscanf(args, "%23s %23s", key, val) != 2) {
    Serial.println(F("usage: set <key> <value>"));
    return;
  }
  float f = atof(val);
  long n = atol(val);
  if (setFloat(key, "warn_low", th.warnLowC, f, -20, 20)) return;
  if (setFloat(key, "warn_high", th.warnHighC, f, 0, 40)) return;
  if (setFloat(key, "crit_low", th.critLowC, f, -30, 20)) return;
  if (setFloat(key, "crit_high", th.critHighC, f, 0, 50)) return;
  if (setFloat(key, "hyst", th.hysteresisC, f, 0.1f, 3.0f)) return;
  if (setFloat(key, "cool_on", th.coolOnC, f, -10, 30)) return;
  if (setFloat(key, "cool_off", th.coolOffC, f, -10, 30)) return;
  if (setU32(key, "warn_latch_ms", th.warningLatchMs, n, 500, 300000)) return;
  if (setU32(key, "crit_latch_ms", th.criticalLatchMs, n, 200, 60000)) return;
  if (setU32(key, "recovery_ms", th.recoveryHoldMs, n, 200, 60000)) return;
  Serial.println(F("unknown key"));
}

static void handleLine(char *line, SystemState &state, Thresholds &th) {
  while (*line == ' ') {
    line++;
  }
  if (*line == '\0') {
    return;
  }
  if (strcmp(line, "help") == 0) {
    printHelp();
    return;
  }
  if (strcmp(line, "status") == 0) {
    cliPrintStatus(state, th);
    return;
  }
  if (strcmp(line, "ack") == 0) {
    zoneAcknowledge(state, millis());
    Serial.println(F("acked (buzzer muted)"));
    return;
  }
  if (strcmp(line, "reset") == 0) {
    zoneReset(state, millis());
    Serial.println(F("reset"));
    return;
  }
  if (strncmp(line, "set ", 4) == 0) {
    handleSet(th, line + 4);
    return;
  }
  Serial.println(F("unknown command; try help"));
}

void cliPoll(SystemState &state, Thresholds &th) {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r') {
      continue;
    }
    if (c == '\n') {
      gLine[gLen] = '\0';
      handleLine(gLine, state, th);
      gLen = 0;
      continue;
    }
    if (gLen + 1 < sizeof(gLine)) {
      gLine[gLen++] = c;
    }
  }
}
