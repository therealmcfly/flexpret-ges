#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

void set_ledmask(const uint8_t byte);

uint8_t pulse_to_ledmask(int16_t value);

#endif // UTIL_H