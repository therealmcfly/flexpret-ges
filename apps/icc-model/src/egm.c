#include "egm.h"

#include <stddef.h>

#include "egm_lut_select.h"

_Static_assert(EGM_LUT_TIMESTEP_MS == ICC_TIMESTEP_MS,
    "EGM lookup timestep does not match ICC timestep");
_Static_assert(EGM_LUT_CELL_COUNT == ICC_NETWORK_1D_CELL_COUNT,
    "EGM lookup cell count does not match network");
_Static_assert(EGM_LUT_PATH_COUNT == ICC_NETWORK_1D_PATH_COUNT,
    "EGM lookup path count does not match network");
_Static_assert(EGM_LUT_STEP_COUNT == EGM_LUT_DELAY_MS / ICC_TIMESTEP_MS,
    "EGM lookup step count does not match delay and timestep");

static bool path_configuration_matches(const IccPath *path)
{
    return path->initialized &&
        path->delay_ms == EGM_LUT_DELAY_MS &&
        path->gap_mm == EGM_LUT_GAP_MM;
}

bool icc_egm_1d5c_configuration_matches(const IccNetwork1d *network)
{
    if (network == NULL || !network->initialized) {
        return false;
    }

    return path_configuration_matches(&network->paths[0]) &&
        path_configuration_matches(&network->paths[1]) &&
        path_configuration_matches(&network->paths[2]) &&
        path_configuration_matches(&network->paths[3]);
}

static IccEgmValue path_contribution(
    const IccPath *path,
    const int32_t lookup[EGM_LUT_DIRECTION_COUNT][EGM_LUT_STEP_COUNT])
{
    if (path->progress_step >= EGM_LUT_STEP_COUNT) {
        return 0;
    }

    switch (path->state) {
    case ICC_PATH_CELL_A_WAIT:
    case ICC_PATH_CELL_A_RELAY:
        return lookup[EGM_LUT_DIRECTION_A_TO_B][path->progress_step];
    case ICC_PATH_CELL_B_WAIT:
    case ICC_PATH_CELL_B_RELAY:
        return lookup[EGM_LUT_DIRECTION_B_TO_A][path->progress_step];
    default:
        return 0;
    }
}

IccEgmValue icc_egm_1d5c_compute(const IccNetwork1d *network)
{
    IccEgmValue result = 0;

    result += path_contribution(&network->paths[0], kEgmPathLookup[0]);
    result += path_contribution(&network->paths[1], kEgmPathLookup[1]);
    result += path_contribution(&network->paths[2], kEgmPathLookup[2]);
    result += path_contribution(&network->paths[3], kEgmPathLookup[3]);

    return result;
}
