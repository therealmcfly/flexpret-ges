#ifndef ICC_MODEL_PATH_H
#define ICC_MODEL_PATH_H

#include <stdbool.h>
#include <stdint.h>

#include "icc.h"

#define ICC_PATH_DEFAULT_DELAY_MS 1000U
#define ICC_PATH_DEFAULT_GAP_MM 6U
#define ICC_PATH_INACTIVE_TIME_MS (-1)

typedef enum {
    ICC_PATH_IDLE = 0,
    ICC_PATH_ANNIHILATE = 1,
    ICC_PATH_CELL_A_WAIT = 2,
    ICC_PATH_CELL_A_RELAY = 3,
    ICC_PATH_CELL_B_WAIT = 4,
    ICC_PATH_CELL_B_RELAY = 5
} IccPathState;

/*
 * Bidirectional connection between two ICC cells.
 *
 * active_time_ms[0] describes propagation from A to B and
 * active_time_ms[1] describes propagation from B to A. An inactive
 * direction contains ICC_PATH_INACTIVE_TIME_MS. The integer times replace
 * the floating-point second values used by iccnet-core.
 */
typedef struct {
    IccPathState state;
    uint32_t elapsed_ms;
    int32_t active_time_ms[2];
    uint16_t delay_ms;
    uint8_t gap_mm;
    Icc *cell_a;
    Icc *cell_b;
    bool initialized;
} IccPath;

void icc_path_init(
    IccPath *path,
    Icc *cell_a,
    Icc *cell_b,
    uint16_t delay_ms,
    uint8_t gap_mm);
void icc_path_step(IccPath *path);
const char *icc_path_state_name(IccPathState state);

#endif
