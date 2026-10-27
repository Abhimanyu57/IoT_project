#include "ui_display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

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
