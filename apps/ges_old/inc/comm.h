#ifndef COMM_H
#define COMM_H

#include <stdint.h>

int16_t recv_from_gut();

void send_to_gut(int16_t value);

void send_et_cycle(int state, uint64_t et_cycle);

void send_et_metrics(int state, uint64_t et_cycle, uint32_t instructions);

#endif // COMM_H