/* Wokwi single-file bundle — generated for simulator compile. */
#include <Arduino.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHTesp.h>
#include <WiFi.h>
#include <PubSubClient.h>


/* ===== config.h ===== */


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

/* ===== types.h ===== */


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

/* ===== sensor_reader.h ===== */


void sensorInit();
void sensorUpdate(uint32_t nowMs, SensorSample &out);
void sensorForceFail(bool inject);
bool sensorFaultInjected();

/* ===== sensor_reader.cpp ===== */


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

/* ===== zone_logic.h ===== */


void zoneInit(SystemState &state);
void zoneEvaluate(SystemState &state, const Thresholds &th, uint32_t nowMs);
void zoneAcknowledge(SystemState &state, uint32_t nowMs);
void zoneReset(SystemState &state, uint32_t nowMs);

/* ===== zone_logic.cpp ===== */

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

/* ===== cooling.h ===== */


void coolingUpdate(SystemState &state, const Thresholds &th);

/* ===== cooling.cpp ===== */

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

/* ===== actuators.h ===== */


void actuatorsInit();
void actuatorsUpdate(const SystemState &state, uint32_t nowMs);
void actuatorsSilenceTone();

/* ===== actuators.cpp ===== */


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

/* ===== ui_display.h ===== */


void displayInit();
void displaySplash();
void displayUpdate(const SystemState &state, const Thresholds &th, bool wifiUp,
                   bool mqttUp, uint32_t nowMs);
void displayShowError(const char *msg);

/* ===== ui_display.cpp ===== */


static Adafruit_SSD1306 gOled(kOledWidth, kOledHeight, &Wire, -1);
static uint32_t gLastDrawMs = 0;
static bool gReady = false;

void displayInit() {
  Wire.begin(kPinOledSda, kPinOledScl);
  gReady = gOled.begin(SSD1306_SWITCHCAPVCC, kOledI2cAddr);
  if (!gReady) {
    return;
  }
  gOled.clearDisplay();
  gOled.setTextColor(SSD1306_WHITE);
  gOled.setTextSize(1);
}

void displaySplash() {
  if (!gReady) {
    return;
  }
  gOled.clearDisplay();
  gOled.setCursor(0, 0);
  gOled.setTextSize(1);
  gOled.println("ColdChain Guardian");
  gOled.println("SIMULATION ONLY");
  gOled.println();
  gOled.println("Not a physical");
  gOled.println("fridge controller.");
  gOled.println();
  gOled.print("fw ");
  gOled.println(kFirmwareVersion);
  gOled.display();
}

void displayShowError(const char *msg) {
  if (!gReady) {
    return;
  }
  gOled.clearDisplay();
  gOled.setCursor(0, 16);
  gOled.println("DISPLAY/INIT");
  gOled.println(msg);
  gOled.display();
}

static void fmtMs(uint32_t ms, char *buf, size_t n) {
  uint32_t s = ms / 1000;
  uint32_t m = s / 60;
  s = s % 60;
  snprintf(buf, n, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

void displayUpdate(const SystemState &state, const Thresholds &th, bool wifiUp,
                   bool mqttUp, uint32_t nowMs) {
  if (!gReady) {
    return;
  }
  if (gLastDrawMs != 0 && (nowMs - gLastDrawMs) < kDisplayPeriodMs) {
    return;
  }
  gLastDrawMs = nowMs;

  gOled.clearDisplay();
  gOled.setTextSize(1);
  gOled.setCursor(0, 0);
  gOled.print("CCG SIM ");
  gOled.println(zoneName(state.zone));

  gOled.setCursor(0, 10);
  if (state.sample.valid) {
    gOled.print("T ");
    gOled.print(state.sample.temperatureC, 1);
    gOled.print("C  H ");
    gOled.print(state.sample.humidityPct, 0);
    gOled.println("%");
  } else {
    gOled.print("T ---.-C  H --%");
    if (state.sample.faultInjected) {
      gOled.println("  INJ");
    } else {
      gOled.println();
    }
  }

  char exc[8];
  fmtMs(state.excursionMs, exc, sizeof(exc));
  gOled.setCursor(0, 20);
  gOled.print("EXC ");
  gOled.print(exc);
  gOled.print("  COOL ");
  gOled.println(state.coolingOn ? "ON" : "OFF");

  gOled.setCursor(0, 30);
  gOled.print("ALM ");
  if (state.persistentAlarm) {
    gOled.print("LATCH");
  } else {
    gOled.print("----");
  }
  if (state.buzzerMuted) {
    gOled.print(" MUTE");
  }
  if (state.recovering) {
    gOled.print(" RCV");
  }
  gOled.println();

  gOled.setCursor(0, 40);
  gOled.print("BAND ");
  gOled.print(th.warnLowC, 0);
  gOled.print("-");
  gOled.print(th.warnHighC, 0);
  gOled.print("C HYS ");
  gOled.print(th.hysteresisC, 1);
  gOled.println();

  gOled.setCursor(0, 50);
  gOled.print(wifiUp ? "WIFI" : "wifi");
  gOled.print(mqttUp ? " MQTT " : " mqtt ");
  gOled.print(state.lastEvent);
  gOled.display();
}

/* ===== inputs.h ===== */


void inputsInit();
void inputsPoll(uint32_t nowMs, SystemState &state, Thresholds &th);
bool inputsFaultInjected();

/* ===== inputs.cpp ===== */


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

/* ===== serial_cli.h ===== */


void cliInit();
void cliPoll(SystemState &state, Thresholds &th);
void cliPrintStatus(const SystemState &state, const Thresholds &th);

/* ===== serial_cli.cpp ===== */


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

/* ===== telemetry.h ===== */


void telemetryInit();
void telemetryUpdate(SystemState &state, Thresholds &th, uint32_t nowMs);
bool telemetryWifiUp();
bool telemetryMqttUp();

/* ===== telemetry.cpp ===== */

#if CCG_ENABLE_MQTT



static WiFiClient gNet;
static PubSubClient gMqtt(gNet);
static uint32_t gLastWifiAttempt = 0;
static uint32_t gLastMqttAttempt = 0;
static uint32_t gLastPub = 0;
static bool gWifiUp = false;
static bool gMqttUp = false;
static Thresholds *gTh = nullptr;
static SystemState *gState = nullptr;

static void onMqtt(char *topic, byte *payload, unsigned int len) {
  char buf[96];
  unsigned int n = (len < sizeof(buf) - 1) ? len : (sizeof(buf) - 1);
  memcpy(buf, payload, n);
  buf[n] = '\0';
  Serial.printf("[mqtt] %s : %s\n", topic, buf);

  if (!gState || !gTh) {
    return;
  }
  if (strcmp(buf, "ack") == 0) {
    zoneAcknowledge(*gState, millis());
    return;
  }
  if (strcmp(buf, "reset") == 0) {
    zoneReset(*gState, millis());
    return;
  }
  if (strncmp(buf, "set ", 4) == 0) {
    // Reuse serial parser path by printing a hint; keep MQTT command simple.
    float v = 0;
    char key[24];
    if (sscanf(buf + 4, "%23s %f", key, &v) == 2) {
      if (strcmp(key, "warn_high") == 0 && v > 0 && v < 40) {
        gTh->warnHighC = v;
      } else if (strcmp(key, "warn_low") == 0 && v > -20 && v < 20) {
        gTh->warnLowC = v;
      }
      setEvent(*gState, "mqtt-set");
    }
  }
}

static void ensureWifi(uint32_t nowMs) {
  if (WiFi.status() == WL_CONNECTED) {
    gWifiUp = true;
    return;
  }
  gWifiUp = false;
  gMqttUp = false;
  if (gLastWifiAttempt != 0 && (nowMs - gLastWifiAttempt) < kWifiRetryMs) {
    return;
  }
  gLastWifiAttempt = nowMs;
  Serial.println(F("[wifi] connecting to Wokwi-GUEST"));
  WiFi.mode(WIFI_STA);
  WiFi.begin(kWifiSsid, kWifiPass, kWifiChannel);
}

static void ensureMqtt(uint32_t nowMs) {
  if (!gWifiUp) {
    return;
  }
  if (gMqtt.connected()) {
    gMqttUp = true;
    gMqtt.loop();
    return;
  }
  gMqttUp = false;
  if (gLastMqttAttempt != 0 && (nowMs - gLastMqttAttempt) < kMqttRetryMs) {
    return;
  }
  gLastMqttAttempt = nowMs;
  char clientId[32];
  snprintf(clientId, sizeof(clientId), "%s%lu", kMqttClientPrefix,
           (unsigned long)(nowMs & 0xFFFF));
  Serial.printf("[mqtt] connect %s as %s\n", kMqttHost, clientId);
  if (gMqtt.connect(clientId)) {
    gMqttUp = true;
    gMqtt.subscribe(kMqttSubTopic);
    Serial.println(F("[mqtt] up"));
  } else {
    Serial.printf("[mqtt] failed rc=%d (control loop still runs)\n",
                  gMqtt.state());
  }
}

static void publish(const SystemState &state, const Thresholds &th,
                    uint32_t nowMs) {
  if (!gMqttUp) {
    return;
  }
  if (gLastPub != 0 && (nowMs - gLastPub) < kTelemetryPeriodMs) {
    return;
  }
  gLastPub = nowMs;

  char json[256];
  snprintf(json, sizeof(json),
           "{\"sim\":true,\"id\":\"%s\",\"t\":%.2f,\"h\":%.1f,\"valid\":%s,"
           "\"zone\":\"%s\",\"alarm\":%s,\"cool\":%s,\"exc_ms\":%lu,"
           "\"fault\":%s,\"mute\":%s,\"warn\":[%.1f,%.1f]}",
           kDeviceId, state.sample.valid ? state.sample.temperatureC : -99.0f,
           state.sample.valid ? state.sample.humidityPct : -1.0f,
           state.sample.valid ? "true" : "false", zoneName(state.zone),
           state.persistentAlarm ? "true" : "false",
           state.coolingOn ? "true" : "false",
           (unsigned long)state.excursionMs,
           state.sample.faultInjected ? "true" : "false",
           state.buzzerMuted ? "true" : "false", th.warnLowC, th.warnHighC);
  gMqtt.publish(kMqttPubTopic, json, false);
}

void telemetryInit() {
  gMqtt.setServer(kMqttHost, kMqttPort);
  gMqtt.setCallback(onMqtt);
  gMqtt.setBufferSize(384);
  Serial.println(F("[mqtt] optional; core control does not wait for network"));
}

void telemetryUpdate(SystemState &state, Thresholds &th, uint32_t nowMs) {
  gState = &state;
  gTh = &th;
  ensureWifi(nowMs);
  if (WiFi.status() == WL_CONNECTED && !gWifiUp) {
    gWifiUp = true;
    Serial.print(F("[wifi] ip "));
    Serial.println(WiFi.localIP());
  }
  ensureMqtt(nowMs);
  publish(state, th, nowMs);
}

bool telemetryWifiUp() { return gWifiUp; }
bool telemetryMqttUp() { return gMqttUp; }

#else

void telemetryInit() {}
void telemetryUpdate(SystemState &, Thresholds &, uint32_t) {}
bool telemetryWifiUp() { return false; }
bool telemetryMqttUp() { return false; }

#endif

/* ===== sketch.ino ===== */
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
