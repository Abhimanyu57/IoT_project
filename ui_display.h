#pragma once

#include "config.h"
#include "types.h"

void displayInit();
void displaySplash();
void displayUpdate(const SystemState &state, const Thresholds &th, bool wifiUp,
                   bool mqttUp, uint32_t nowMs);
void displayShowError(const char *msg);
