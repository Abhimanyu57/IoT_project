#pragma once

#include "config.h"
#include "types.h"

void zoneInit(SystemState &state);
void zoneEvaluate(SystemState &state, const Thresholds &th, uint32_t nowMs);
void zoneAcknowledge(SystemState &state, uint32_t nowMs);
void zoneReset(SystemState &state, uint32_t nowMs);
