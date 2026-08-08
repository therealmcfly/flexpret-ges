#ifndef ICC_MODEL_NETWORK_H
#define ICC_MODEL_NETWORK_H

#include <stdbool.h>
#include <stdint.h>

#include "icc.h"
#include "path.h"

#define ICC_NETWORK_1D_CELL_COUNT 5U
#define ICC_NETWORK_1D_PATH_COUNT (ICC_NETWORK_1D_CELL_COUNT - 1U)

/* Static one-dimensional ICC chain: path i connects cell i to cell i + 1. */
typedef struct {
    Icc cells[ICC_NETWORK_1D_CELL_COUNT];
    IccPath paths[ICC_NETWORK_1D_PATH_COUNT];
    bool initialized;
} IccNetwork1d;

bool icc_network_1d_init(
    IccNetwork1d *network,
    const int8_t cell_intervals_s[ICC_NETWORK_1D_CELL_COUNT],
    const uint16_t path_delays_ms[ICC_NETWORK_1D_PATH_COUNT],
    const uint8_t path_gaps_mm[ICC_NETWORK_1D_PATH_COUNT]);

/* Update all cells first, then all paths, once per 200 ms model period. */
void icc_network_1d_step(IccNetwork1d *network);

#endif
