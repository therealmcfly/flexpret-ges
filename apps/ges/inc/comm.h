#ifndef COMM_H
#define COMM_H

#include <stdint.h>

int recv_from_gut();

void send_to_gut(int value);

void send_et_metrics(int state, uint32_t et_ns, uint32_t instructions);

#endif // COMM_H