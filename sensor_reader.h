#pragma once

#include "config.h"
#include "types.h"

void sensorInit();
void sensorUpdate(uint32_t nowMs, SensorSample &out);
void sensorForceFail(bool inject);
bool sensorFaultInjected();
