#ifndef COMM_H
#define COMM_H

#include <stdint.h>

int16_t recv_from_gut();

void send_to_gut(int16_t value);

#endif // COMM_H