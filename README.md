# ColdChain Guardian

Virtual IoT cold-storage **monitor and cooling-control** node.

**This is a Wokwi simulation, not a physically tested refrigeration or medical-device controller.** Do not use these thresholds, alarms, or relay logic on real vaccines, food, or hardware.

The ESP32 firmware reads a **real virtual DHT22** in Wokwi, drives **real virtual** OLED, LEDs, buzzer, and relay, and reacts when you move the DHT22 temperature slider. There is no Python script inventing sensor values and no dashboard of fabricated data.

---

## What you do here vs over there

I built the circuit definition and C++ firmware **in this folder**. I cannot press Wokwi’s Start button inside your browser session, and I cannot inject temperature from Cursor.

**Over here (this repo):** firmware, `diagram.json`, thresholds, serial CLI.

**Over there (Wokwi in the browser):** compile, run, move the DHT22 slider, flip the fault switch, watch LEDs/relay/OLED.

**Optional extra (only if you want MQTT on a phone/PC):** connect a client to HiveMQ’s public broker. That path is **not** the core controller.

OpenCode is optional. The project is already implemented; you do not need it unless you want another assistant to edit files.

---

## Architecture

```
DHT22 (Wokwi slider)
        |
        v
  sensor_reader.cpp  -- fail streak / inject switch
        |
        v
  zone_logic.cpp     -- hysteresis, excursion timer, latch, recovery
        |                    |
        |                    v
        |             cooling.cpp  -- relay hysteresis
        v
  actuators.cpp  LEDs + buzzer + relay
  ui_display.cpp OLED
  serial_cli.cpp Serial
  telemetry.cpp  Wi-Fi + MQTT (optional, non-blocking)
```

The control loop in `sketch.ino` **never waits** for Wi-Fi. If MQTT is down, cooling and alarms still run.

---

## Simulated hardware (`diagram.json`)

| Part | Wokwi type | ESP32 pin | Role |
|---|---|---|---|
| ESP32 DevKit C v4 | `board-esp32-devkit-c-v4` | — | MCU |
| DHT22 | `wokwi-dht22` | GPIO 4 (SDA) | Temperature / humidity |
| SSD1306 OLED 128×64 | `board-ssd1306` | SDA 21, SCL 22 | Live UI |
| Green / yellow / red LEDs | `wokwi-led` + 220 Ω | 25 / 26 / 27 | Zone |
| Piezo buzzer | `wokwi-buzzer` | GPIO 14 | Audible alarm |
| Relay (PNP / active-high) | `wokwi-relay-module` | GPIO 13 (IN) | Simulated compressor |
| Blue LED on relay NO | LED + 220 Ω from 5 V | — | Cooling output visible |
| ACK button (key **A**) | `wokwi-pushbutton` | GPIO 32 | Mute buzzer |
| Fault switch | `wokwi-slide-switch` | GPIO 33 | Inject sensor failure |

**Fault switch:** leave it **RIGHT** for a healthy sensor. Slide **LEFT** to inject failure (GPIO 33 pulled to GND).

Relay uses `"transistor": "pnp"` so **GPIO 13 HIGH = cooling ON** (COM connected to NO, blue LED lights).

---

## Firmware logic

1. Sample DHT22 about once per second (`DHTesp`). Invalid CRC / NaN increments a fail streak.
2. If the fail streak hits 3, **or** the fault switch is left, zone becomes `FAULT`. Cooling stays **ON** (fail-safe for this sim). Alarm latches.
3. Valid temperature is classified with **hysteresis**:
   - Enter **WARNING** at ≤ 2 °C or ≥ 8 °C
   - Enter **CRITICAL** at ≤ 0 °C or ≥ 10 °C
   - Leave a worse zone only after crossing back by **0.5 °C**
4. **Excursion timer** starts when the zone leaves `NORMAL` (or on fault). OLED shows `EXC mm:ss`.
5. **Persistent alarm**
   - WARNING held ≥ 15 s → latch
   - CRITICAL held ≥ 3 s → latch
   - Sensor fault → latch immediately
6. **ACK** mutes the buzzer; the latch and red/yellow LEDs remain.
7. **Automatic recovery:** temperature must stay in the normal band (after hysteresis) for **4 s**, then latch, excursion timer, and mute flags clear. Green LED blinks during the recovery hold.
8. **Cooling relay** (independent hysteresis): ON at ≥ 6 °C, OFF at ≤ 4 °C. Default DHT22 is 5 °C so the compressor starts **off**.

---

## Default thresholds

| Parameter | Default | Serial `set` key |
|---|---|---|
| Warning low / high | 2 / 8 °C | `warn_low` `warn_high` |
| Critical low / high | 0 / 10 °C | `crit_low` `crit_high` |
| Zone hysteresis | 0.5 °C | `hyst` |
| Cool ON / OFF | 6 / 4 °C | `cool_on` `cool_off` |
| Warning latch | 15000 ms | `warn_latch_ms` |
| Critical latch | 3000 ms | `crit_latch_ms` |
| Recovery hold | 4000 ms | `recovery_ms` |

Serial (115200): `help`, `status`, `ack`, `reset`, `set warn_high 9`.

---

## Setup (you do this in Wokwi)

1. Open [https://wokwi.com](https://wokwi.com) and sign in if you want to save.
2. **New project → ESP32** (Arduino).
3. In the Wokwi editor, replace these files with copies from this folder:
   - `diagram.json`
   - `sketch.ino`
   - `libraries.txt`
   - every `.h` and `.cpp` file in this folder (add extra tabs: **Add File** / paste).
4. Confirm Library Manager picked:
   - `DHT sensor library for ESPx`
   - `Adafruit SSD1306` + `Adafruit GFX Library` + `Adafruit BusIO`
   - `PubSubClient`
5. Click the **green play** button. Wait until the OLED shows `CCG SIM NORMAL` and Serial prints `loop start`.
6. Click the **DHT22** body. Use the **temperature slider**. Do not edit a Python script.

Wokwi VS Code extension is optional. The browser path is enough.

If a `.cpp` tab does not compile, put that file in the same project as `sketch.ino` (same trick as Arduino IDE: all `.cpp` next to the sketch).

---

## Test scenarios (Wokwi)

Start each scenario from **~5 °C**, fault switch **RIGHT**, after `auto-recovered` or Serial `reset`.

### 1. Normal operation

- DHT22 ≈ 5 °C.
- **Expect:** green LED on, yellow/red off, OLED `NORMAL`, `COOL OFF` (5 °C is between 4 and 6), buzzer silent.

### 2. Temperature above the safe range (warning)

- Slide temperature to **~8.5 °C**.
- **Expect:** yellow LED, OLED `WARNING`, excursion timer counting, slow buzzer, cooling **ON** (temp ≥ 6 °C).

### 3. Prolonged exposure (persistent alarm)

- Keep ~8.5 °C for **more than 15 s**.
- **Expect:** `ALM LATCH` on OLED, faster alarm tone.
- Press **ACK** (or keyboard **A**). **Expect:** buzzer stops, latch remains.

### 4. Critical excursion

- Slide to **~11 °C** (or below **0 °C**).
- **Expect:** red LED (yellow may blink), `CRITICAL`, latch within ~3 s, cooling ON.

### 5. Sensor failure

- Slide the **fault switch LEFT** (or hold it in the left position).
- **Expect:** OLED `FAULT` / `T ---.-C`, red LED blinking, alarm, cooling ON even if the last temperature was in-band.

### 6. Recovery after return to normal

- Switch **RIGHT** again (if you used fault).
- Slide temperature to **~5 °C** and wait **> 4 s** in-band (if you came from warning, you must pass hysteresis: from 8.5 °C go to **≤ 7.5 °C**, then to 5 °C).
- **Expect:** green LED blinks during `RCV`, then `NORMAL`, `ALM ----`, buzzer off, excursion `00:00`. Cooling turns **OFF** once temp ≤ 4 °C; at 5 °C it stays in whatever state hysteresis left (ON until 4 °C). To see cooling drop, dip to **~3.5 °C** then back to 5 °C.

---

## Optional Wi-Fi / MQTT (separate from control)

Wokwi provides open AP **`Wokwi-GUEST`** (no password, channel 6). Firmware publishes JSON to:

- Broker: `broker.hivemq.com:1883`
- Publish: `coldchain/guardian/telemetry`
- Subscribe: `coldchain/guardian/cmd` (`ack`, `reset`, `set warn_high 9`)

**You, on your machine (optional):**

1. Open [http://www.hivemq.com/demos/websocket-client/](http://www.hivemq.com/demos/websocket-client/) (or any MQTT client).
2. Connect to the public broker.
3. Subscribe to `coldchain/guardian/telemetry`.
4. Confirm JSON updates when you move the DHT22 slider.

If this fails (public broker / Wokwi gateway limits), **ignore it**. Hardware control is already demonstrated on the OLED and LEDs.

To compile without MQTT, set `#define CCG_ENABLE_MQTT 0` in `config.h`.

---

## File map

| File | Responsibility |
|---|---|
| `sketch.ino` | Setup / loop glue |
| `config.h` | Pins and default thresholds |
| `types.h` | Zone and system state |
| `sensor_reader.*` | DHT22 + fail injection |
| `zone_logic.*` | Hysteresis, latch, recovery |
| `cooling.*` | Relay setpoints |
| `actuators.*` | LEDs, buzzer, relay pin |
| `ui_display.*` | OLED |
| `inputs.*` | ACK + fault switch |
| `serial_cli.*` | Runtime config |
| `telemetry.*` | Optional Wi-Fi/MQTT |
| `diagram.json` | Circuit |
| `libraries.txt` | Wokwi libraries |

---

## License / disclaimer

Course / demo simulation only. No warranty. Not for clinical, food-safety, or production control.
