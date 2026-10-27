#pragma once

#include "types.h"

void actuatorsInit();
void actuatorsUpdate(const SystemState &state, uint32_t nowMs);
void actuatorsSilenceTone();
