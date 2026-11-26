#ifndef PUMP_H
#define PUMP_H

#include <stdbool.h>

#define THRESHOLD_L 10   // pump turns on below this soil %
#define THRESHOLD_H 25   // pump turns off above this soil %


void pump_task(void *pvParameter);

#endif

