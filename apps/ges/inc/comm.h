#ifndef COMM_H
#define COMM_H

#include <stdint.h>
#include "global.h"

int recv_from_gut(void);

void send_to_gut(PmState value);

void send_et_metrics(int state, int et_ns, int cycles, int instructions);

#endif // COMM_H