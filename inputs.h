#pragma once

#include "types.h"

void inputsInit();
void inputsPoll(uint32_t nowMs, SystemState &state, Thresholds &th);
bool inputsFaultInjected();
