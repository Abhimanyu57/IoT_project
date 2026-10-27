#pragma once

#include "config.h"
#include "types.h"

void telemetryInit();
void telemetryUpdate(SystemState &state, Thresholds &th, uint32_t nowMs);
bool telemetryWifiUp();
bool telemetryMqttUp();
