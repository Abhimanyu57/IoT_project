#pragma once

#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cctype>
#include <cstdarg>

using std::isnan;

typedef uint8_t byte;

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LED_BUILTIN 2

#define F(x) x

struct FakeSerial {
  void begin(unsigned long) {}
  void println() { std::puts(""); }
  void println(const char *s) { std::puts(s); }
  int printf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vprintf(fmt, ap);
    va_end(ap);
    return n;
  }
  int available() { return 0; }
  int read() { return -1; }
  template <typename T>
  void print(T) {}
  void println(int) {}
};

static FakeSerial Serial;

inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}
inline void pinMode(int, int) {}
inline int digitalRead(int) { return HIGH; }
inline void digitalWrite(int, int) {}
inline void tone(int, int) {}
inline void noTone(int) {}
