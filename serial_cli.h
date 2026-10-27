#pragma once

#include "config.h"
#include "types.h"

void cliInit();
void cliPoll(SystemState &state, Thresholds &th);
void cliPrintStatus(const SystemState &state, const Thresholds &th);
