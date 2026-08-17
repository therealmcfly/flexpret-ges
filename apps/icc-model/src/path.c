#include "path.h"

#include <stddef.h>

static void clear_active_times(IccPath *path)
{
    path->active_time_ms[0] = ICC_PATH_INACTIVE_TIME_MS;
    path->active_time_ms[1] = ICC_PATH_INACTIVE_TIME_MS;
}

/*
 * A relay set during this step is consumed by the destination ICC during the
 * next 200 ms model step. Request it one step before the nominal delay so the
 * effective activation delay remains delay_ms rather than delay_ms + 200 ms.
 */
static bool relay_is_due_next_step(const IccPath *path)
{
    const uint32_t delay_ms = (uint32_t)path->delay_ms;

    if (path->elapsed_ms >= delay_ms) {
        return true;
    }

    return ICC_TIMESTEP_MS >= delay_ms - path->elapsed_ms;
}

void icc_path_init(
    IccPath *path,
    Icc *cell_a,
    Icc *cell_b,
    uint16_t delay_ms,
    uint8_t gap_mm)
{
    if (path == NULL) {
        return;
    }

    path->state = ICC_PATH_IDLE;
    path->elapsed_ms = 0U;
    path->progress_step = 0U;
    clear_active_times(path);
    path->delay_ms = delay_ms > 0U
        ? delay_ms : ICC_PATH_DEFAULT_DELAY_MS;
    path->gap_mm = gap_mm > 0U
        ? gap_mm : ICC_PATH_DEFAULT_GAP_MM;
    path->cell_a = cell_a;
    path->cell_b = cell_b;
    path->initialized = true;
}

void icc_path_step(IccPath *path)
{
    if (path == NULL || !path->initialized ||
        path->cell_a == NULL || path->cell_b == NULL) {
        return;
    }

    switch (path->state) {
    case ICC_PATH_IDLE:
        if (icc_is_active(path->cell_a) &&
            icc_is_active(path->cell_b)) {
            path->progress_step = 0U;
            path->state = ICC_PATH_ANNIHILATE;
        } else if (icc_is_active(path->cell_a)) {
            path->elapsed_ms = 0U;
            path->progress_step = 0U;
            path->active_time_ms[0] = 0;
            path->active_time_ms[1] = ICC_PATH_INACTIVE_TIME_MS;
            path->state = ICC_PATH_CELL_A_WAIT;
        } else if (icc_is_active(path->cell_b)) {
            path->elapsed_ms = 0U;
            path->progress_step = 0U;
            path->active_time_ms[0] = ICC_PATH_INACTIVE_TIME_MS;
            path->active_time_ms[1] = 0;
            path->state = ICC_PATH_CELL_B_WAIT;
        }
        break;

    case ICC_PATH_ANNIHILATE:
        if (!icc_is_active(path->cell_a) &&
            !icc_is_active(path->cell_b)) {
            path->state = ICC_PATH_IDLE;
            path->elapsed_ms = 0U;
            path->progress_step = 0U;
            clear_active_times(path);
        }
        break;

    case ICC_PATH_CELL_A_WAIT:
        path->elapsed_ms += ICC_TIMESTEP_MS;
        path->progress_step++;
        path->active_time_ms[0] = (int32_t)path->elapsed_ms;
        if (relay_is_due_next_step(path)) {
            icc_stimulate(path->cell_b);
            path->state = ICC_PATH_CELL_A_RELAY;
        }
        break;

    case ICC_PATH_CELL_A_RELAY:
        path->state = ICC_PATH_IDLE;
        path->elapsed_ms = 0U;
        path->progress_step = 0U;
        clear_active_times(path);
        break;

    case ICC_PATH_CELL_B_WAIT:
        path->elapsed_ms += ICC_TIMESTEP_MS;
        path->progress_step++;
        path->active_time_ms[1] = (int32_t)path->elapsed_ms;
        if (relay_is_due_next_step(path)) {
            icc_stimulate(path->cell_a);
            path->state = ICC_PATH_CELL_B_RELAY;
        }
        break;

    case ICC_PATH_CELL_B_RELAY:
        path->state = ICC_PATH_IDLE;
        path->elapsed_ms = 0U;
        path->progress_step = 0U;
        clear_active_times(path);
        break;

    default:
        path->state = ICC_PATH_IDLE;
        path->elapsed_ms = 0U;
        path->progress_step = 0U;
        clear_active_times(path);
        break;
    }
}

const char *icc_path_state_name(IccPathState state)
{
    switch (state) {
    case ICC_PATH_IDLE:
        return "IDLE";
    case ICC_PATH_ANNIHILATE:
        return "ANNIHILATE";
    case ICC_PATH_CELL_A_WAIT:
        return "A_WAIT";
    case ICC_PATH_CELL_A_RELAY:
        return "A_RELAY";
    case ICC_PATH_CELL_B_WAIT:
        return "B_WAIT";
    case ICC_PATH_CELL_B_RELAY:
        return "B_RELAY";
    default:
        return "UNKNOWN";
    }
}
