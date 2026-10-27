#!/usr/bin/env python3
"""Generate ColdChain Guardian project-summary PDF."""

from pathlib import Path

from fpdf import FPDF

FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"
FONT_B = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
FONT_M = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
OUT = Path(__file__).resolve().parents[1] / "ColdChain_Guardian_Project_Summary.pdf"

NAVY = (18, 42, 66)
TEAL = (0, 110, 120)
RULE = (200, 208, 214)
MUTED = (90, 98, 106)


class Doc(FPDF):
    def header(self):
        if self.page_no() == 1:
            return
        self.set_font("Sans", "", 8)
        self.set_text_color(*MUTED)
        self.cell(0, 8, "ColdChain Guardian  |  Project summary  |  Simulation only", align="L")
        self.ln(10)

    def footer(self):
        self.set_y(-14)
        self.set_font("Sans", "", 8)
        self.set_text_color(*MUTED)
        self.cell(0, 8, f"{self.page_no()}", align="C")

    def h1(self, t):
        self.set_font("Sans", "B", 13)
        self.set_text_color(*NAVY)
        self.cell(0, 9, t, new_x="LMARGIN", new_y="NEXT")
        self.set_draw_color(*TEAL)
        self.set_line_width(0.5)
        self.line(self.l_margin, self.get_y(), self.w - self.r_margin, self.get_y())
        self.ln(4)

    def h2(self, t):
        self.set_font("Sans", "B", 11)
        self.set_text_color(*TEAL)
        self.cell(0, 8, t, new_x="LMARGIN", new_y="NEXT")
        self.ln(1)

    def body(self, t):
        self.set_font("Sans", "", 10)
        self.set_text_color(30, 30, 30)
        self.multi_cell(0, 5.2, t)
        self.ln(2)

    def bullet(self, t):
        self.set_font("Sans", "", 10)
        self.set_text_color(30, 30, 30)
        x = self.get_x()
        self.cell(6, 5.2, "•")
        self.multi_cell(0, 5.2, t)
        self.set_x(x)

    def numbered(self, n, t):
        self.set_font("Sans", "", 10)
        self.set_text_color(30, 30, 30)
        x = self.get_x()
        self.cell(8, 5.2, f"{n}.")
        self.multi_cell(0, 5.2, t)
        self.set_x(x)

    def code(self, t):
        self.set_fill_color(245, 247, 248)
        self.set_font("Mono", "", 8.5)
        self.set_text_color(20, 20, 20)
        self.multi_cell(0, 4.6, t, fill=True)
        self.ln(3)

    def table(self, headers, rows, col_w):
        self.set_font("Sans", "B", 8.5)
        self.set_fill_color(18, 42, 66)
        self.set_text_color(255, 255, 255)
        for h, w in zip(headers, col_w):
            self.cell(w, 7, " " + h, fill=True, border=0)
        self.ln()
        self.set_font("Sans", "", 8.5)
        fill = False
        for row in rows:
            if self.get_y() + 8 > self.page_break_trigger:
                self.add_page()
            if fill:
                self.set_fill_color(238, 244, 245)
            else:
                self.set_fill_color(255, 255, 255)
            self.set_text_color(30, 30, 30)
            for cell, w in zip(row, col_w):
                self.cell(w, 6.5, " " + cell, fill=True, border=0)
            self.ln()
            fill = not fill
        self.ln(3)


def main():
    pdf = Doc(format="A4")
    pdf.set_auto_page_break(auto=True, margin=16)
    pdf.add_font("Sans", "", FONT)
    pdf.add_font("Sans", "B", FONT_B)
    pdf.add_font("Mono", "", FONT_M)
    pdf.set_margins(16, 16, 16)
    pdf.add_page()

    # Cover band
    pdf.set_fill_color(*NAVY)
    pdf.rect(0, 0, 210, 48, "F")
    pdf.set_xy(16, 14)
    pdf.set_font("Sans", "B", 22)
    pdf.set_text_color(255, 255, 255)
    pdf.cell(0, 10, "ColdChain Guardian", new_x="LMARGIN", new_y="NEXT")
    pdf.set_x(16)
    pdf.set_font("Sans", "", 12)
    pdf.cell(0, 8, "Virtual IoT cold-storage monitor and cooling controller", new_x="LMARGIN", new_y="NEXT")
    pdf.set_x(16)
    pdf.set_font("Sans", "", 9)
    pdf.set_text_color(180, 210, 214)
    pdf.cell(0, 6, "Project summary  ·  Firmware v1.0.0-sim  ·  Wokwi ESP32 simulation")
    pdf.set_y(56)

    pdf.set_fill_color(255, 236, 200)
    pdf.set_text_color(90, 50, 0)
    pdf.set_font("Sans", "B", 9)
    pdf.multi_cell(
        0,
        5,
        "  DISCLAIMER: This is a software simulation in Wokwi. It is not a physically tested "
        "refrigeration unit, not a medical device, and must not be used to control real vaccines, "
        "food, or hardware.",
        fill=True,
    )
    pdf.ln(6)

    pdf.h1("1. What this project is")
    pdf.body(
        "ColdChain Guardian is a virtual Internet-of-Things (IoT) node that watches a cold room "
        "and drives a simulated compressor. An ESP32 microcontroller reads a DHT22 temperature/"
        "humidity sensor, shows live status on an OLED screen, classifies the temperature as "
        "normal / warning / critical, sounds a buzzer, lights status LEDs, and switches a relay "
        "that stands in for cooling."
    )
    pdf.body(
        "The hardware does not exist as a purchased kit. Everything runs in Wokwi, a free browser "
        "simulator: the ESP32, the sensor, the display, the buzzer, the LEDs, and the relay are "
        "all virtual parts wired in diagram.json. When you drag the DHT22 temperature slider, the "
        "firmware reads that value over the simulated data pin. There is no Python script inventing "
        "numbers and no fake dashboard."
    )
    pdf.body(
        "There is no phone app and no desktop window besides the Wokwi page (circuit on the right, "
        "code on the left, Serial monitor at the bottom)."
    )

    pdf.h1("2. What you received")
    for b in [
        "ESP32 firmware in C++ (Arduino), split into sketch.ino plus .h/.cpp modules.",
        "Circuit definition: diagram.json (ESP32 DevKit C v4, DHT22, SSD1306 OLED, LEDs, buzzer, relay, ACK button, fault switch).",
        "libraries.txt listing Wokwi Arduino libraries.",
        "Host unit tests for zone classification, latching, recovery, and cooling hysteresis.",
        "This summary PDF and README.md.",
    ]:
        pdf.bullet(b)
    pdf.ln(2)

    pdf.h1("3. How to run it (Wokwi)")
    pdf.body("You need a browser and an internet connection. No physical board. No paid Wokwi plan is required (free builds may wait in a queue).")
    for i, t in enumerate(
        [
            "Open https://wokwi.com and choose New project → ESP32 (Arduino).",
            "Sign in if you want to Save and get a shareable project URL.",
            "Replace diagram.json, sketch.ino, and libraries.txt with the copies from this folder.",
            "Add File for every remaining .h and .cpp in the folder (same names). Arduino/Wokwi compile all of them together, like the Arduino IDE.",
            "Confirm Library Manager has: DHT sensor library for ESPx; Adafruit SSD1306; Adafruit GFX Library; Adafruit BusIO; PubSubClient.",
            "Press the green Play button. Wait for compile (can take a minute, plus a queue if you are not on a paid plan).",
            "Success looks like: OLED “CCG SIM NORMAL”, green LED on, Serial “loop start” and STATUS zone=NORMAL T=5.00.",
            "Click the DHT22 body (not a Python file). Use the temperature slider. Keyboard A presses ACK. Fault switch LEFT injects sensor failure; leave it RIGHT for a healthy sensor.",
        ],
        start=1,
    ):
        pdf.numbered(i, t)
    pdf.ln(2)
    pdf.body(
        "If a .cpp file does not compile, it is not next to sketch.ino — put all firmware files in the same Wokwi project. "
        "DHT22 data must be on GPIO 15 (Wokwi’s ESP32 DHT examples). GPIO 4 does not decode the sensor in this simulator."
    )

    pdf.h1("4. Simulated hardware and pins")
    usable = pdf.w - pdf.l_margin - pdf.r_margin
    pdf.table(
        ["Part", "Wokwi type", "ESP32", "Role"],
        [
            ["ESP32 DevKit C v4", "board-esp32-devkit-c-v4", "—", "Microcontroller"],
            ["DHT22", "wokwi-dht22", "GPIO 15", "Temperature / humidity"],
            ["SSD1306 OLED 128×64", "board-ssd1306", "SDA 21, SCL 22", "Live UI (I2C 0x3C)"],
            ["Green / yellow / red LEDs", "wokwi-led + 220 Ω", "25 / 26 / 27", "OK / warning / critical"],
            ["Piezo buzzer", "wokwi-buzzer", "GPIO 14", "Audible alarm"],
            ["Relay (PNP, active-high)", "wokwi-relay-module", "GPIO 13 IN", "Simulated compressor"],
            ["Blue LED on relay NO", "LED + 220 Ω from 5 V", "via relay", "Cooling visible"],
            ["ACK button (key A)", "wokwi-pushbutton", "GPIO 32", "Mute buzzer"],
            ["Fault switch", "wokwi-slide-switch", "GPIO 33", "Inject sensor failure"],
        ],
        [48, 48, 32, usable - 128],
    )
    pdf.body(
        "Relay: transistor = pnp so GPIO 13 HIGH means cooling ON (COM connected to NO, blue LED lights). "
        "Fault switch RIGHT = healthy (GPIO 33 pulled up). LEFT shorts GPIO 33 to GND = injected failure."
    )

    pdf.h1("5. Firmware behaviour")
    pdf.body("The loop in sketch.ino never waits for Wi-Fi. Cooling and alarms keep running if MQTT is down.")
    for i, t in enumerate(
        [
            "Read DHT22 about once per second (DHTesp). Invalid CRC/NaN increments a fail streak.",
            "Three failed reads, or the fault switch left, → zone FAULT. Cooling stays ON (fail-safe). Alarm latches.",
            "Valid temperature: WARNING at ≤ 2 °C or ≥ 8 °C; CRITICAL at ≤ 0 °C or ≥ 10 °C. Leaving a worse zone needs 0.5 °C hysteresis.",
            "Excursion timer starts when the zone leaves NORMAL (or on fault). OLED shows EXC mm:ss.",
            "Persistent alarm: warning held ≥ 15 s, or critical held ≥ 3 s, or any sensor fault.",
            "ACK mutes the buzzer only. Latch and LEDs remain until recovery.",
            "Auto-recovery: temperature must stay in-band (after hysteresis) for 4 s. Then latch, timer, and mute clear. Green LED blinks during the hold.",
            "Cooling relay is separate hysteresis: ON at ≥ 6 °C, OFF at ≤ 4 °C. Default sensor is 5 °C, so cooling starts OFF.",
        ],
        start=1,
    ):
        pdf.numbered(i, t)
    pdf.ln(2)

    pdf.h1("6. Default thresholds")
    pdf.table(
        ["Parameter", "Default", "Serial set key"],
        [
            ["Warning low / high", "2 / 8 °C", "warn_low  warn_high"],
            ["Critical low / high", "0 / 10 °C", "crit_low  crit_high"],
            ["Zone hysteresis", "0.5 °C", "hyst"],
            ["Cool ON / OFF", "6 / 4 °C", "cool_on  cool_off"],
            ["Warning latch", "15000 ms", "warn_latch_ms"],
            ["Critical latch", "3000 ms", "crit_latch_ms"],
            ["Recovery hold", "4000 ms", "recovery_ms"],
        ],
        [55, 40, usable - 95],
    )
    pdf.body("Serial is 115200 baud. Commands: help, status, ack, reset, set warn_high 9.")

    pdf.h1("7. Demo scenarios (what to show)")
    pdf.body("Start from ~5 °C, fault switch RIGHT, after auto-recovered or Serial reset.")
    pdf.table(
        ["Scenario", "What you do", "What you should see"],
        [
            ["Normal", "Leave ~5 °C", "Green LED, OLED NORMAL, COOL OFF, quiet buzzer"],
            ["Warning", "Slide to ~8.5 °C", "Yellow LED, WARNING, timer counts, cooling ON"],
            ["Prolonged", "Hold 8.5 °C > 15 s, then A / ACK", "ALM LATCH, then mute with latch still on"],
            ["Critical", "Slide to ~11 °C (or below 0 °C)", "Red LED, CRITICAL, latch in ~3 s, cooling ON"],
            ["Sensor fail", "Fault switch LEFT", "FAULT, T ---, red blink, cooling ON"],
            ["Recovery", "Switch RIGHT, slide to ~5 °C, wait > 4 s", "Green blink RCV, then NORMAL, latch cleared"],
        ],
        [32, 58, usable - 90],
    )
    pdf.body(
        "Hysteresis note: leaving warning at 8.5 °C requires dropping to ≤ 7.5 °C. Cooling stays ON until ≤ 4 °C; "
        "at 5 °C after a hot excursion it may still be ON until you dip to ~3.5 °C."
    )

    pdf.h1("8. Optional Wi-Fi / MQTT")
    pdf.body(
        "This is extra. The assignment still passes on OLED/LEDs/relay if MQTT fails. "
        "Wokwi open AP: SSID Wokwi-GUEST, no password, channel 6."
    )
    pdf.bullet("Broker: broker.hivemq.com port 1883")
    pdf.bullet("Publish: coldchain/guardian/telemetry  (JSON, sim:true)")
    pdf.bullet("Subscribe: coldchain/guardian/cmd  (ack, reset, set warn_high 9)")
    pdf.ln(1)
    pdf.body(
        "On a laptop you can use the HiveMQ public websocket client, connect, and subscribe to the telemetry topic. "
        "To compile without MQTT, set CCG_ENABLE_MQTT to 0 in config.h."
    )

    pdf.h1("9. Source files")
    pdf.table(
        ["File", "Role"],
        [
            ["sketch.ino", "setup() / loop() glue"],
            ["config.h", "Pins and default thresholds"],
            ["types.h", "Zone enum and system state"],
            ["sensor_reader.*", "DHT22 + fail injection"],
            ["zone_logic.*", "Hysteresis, latch, recovery"],
            ["cooling.*", "Relay setpoints"],
            ["actuators.*", "LEDs, buzzer, relay pin"],
            ["ui_display.*", "OLED"],
            ["inputs.*", "ACK + fault switch"],
            ["serial_cli.*", "Runtime config over Serial"],
            ["telemetry.*", "Optional Wi-Fi / MQTT"],
            ["diagram.json", "Circuit"],
            ["libraries.txt", "Wokwi libraries"],
            ["tests/test_zone.cpp", "Host tests (no ESP32)"],
        ],
        [45, usable - 45],
    )
    pdf.body("Host tests (no board):")
    pdf.code(
        "g++ -std=c++17 -I. -Itests/arduino_stub tests/test_zone.cpp "
        "zone_logic.cpp cooling.cpp -o /tmp/test_zone && /tmp/test_zone"
    )

    pdf.h1("10. What was verified")
    pdf.bullet(
        "Host tests: normal, warning, hysteresis, 15 s latch, ACK mute, critical, 4 s recovery, "
        "sensor-fault failsafe cooling, relay 6/4 °C. All passed."
    )
    pdf.bullet(
        "Wokwi: firmware compiled and ran. At 5 °C: NORMAL, green LED, OLED live, cooling off, "
        "Wi-Fi Wokwi-GUEST and MQTT connected."
    )
    pdf.bullet("DHT22 on GPIO 4 did not decode in Wokwi; GPIO 15 does. That is the shipping pin map.")
    pdf.ln(2)

    pdf.h1("11. What this is not")
    pdf.bullet("Not a tested physical fridge or vaccine refrigerator.")
    pdf.bullet("Not a mobile app. Not a cloud product. MQTT is a demo on a public broker.")
    pdf.bullet("No hardware was purchased. Do not load this firmware onto a real compressor controller.")
    pdf.ln(2)
    pdf.body("Course / demo simulation only. No warranty.")

    pdf.output(OUT)
    print(OUT)


if __name__ == "__main__":
    main()
