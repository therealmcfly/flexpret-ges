#include "egm.h"

#include <limits.h>
#include <stddef.h>

#include "egm_relative_lut.h"

#define EGM_NETWORK_MIN_UNITS 0
#define EGM_NETWORK_MAX_UNITS 400
#define EGM_PATH_GAP_MM 6U
#define EGM_PATH_DELAY_MS 1000U
#define EGM_PATH_LENGTH_UNITS 100
#define EGM_RELATIVE_MIN_UNITS (-400)
#define EGM_RELATIVE_MAX_UNITS 400

#if ICC_TIMESTEP_MS == 10U
#define EGM_PROGRESS_STRIDE_UNITS 1
#elif ICC_TIMESTEP_MS == 20U
#define EGM_PROGRESS_STRIDE_UNITS 2
#elif ICC_TIMESTEP_MS == 50U
#define EGM_PROGRESS_STRIDE_UNITS 5
#elif ICC_TIMESTEP_MS == 100U
#define EGM_PROGRESS_STRIDE_UNITS 10
#elif ICC_TIMESTEP_MS == 200U
#define EGM_PROGRESS_STRIDE_UNITS 20
#else
#error "EGM supports ICC timesteps of 10, 20, 50, 100, or 200 ms"
#endif

#define EGM_PATH_STEP_COUNT \
    (EGM_PATH_LENGTH_UNITS / EGM_PROGRESS_STRIDE_UNITS)

_Static_assert(EGM_LUT_POSITION_STEP_UM == 60U,
    "runtime coordinate unit must match LUT spatial step");
_Static_assert(EGM_LUT_RELATIVE_MIN_UM == -24000,
    "runtime minimum relative position must match LUT");
_Static_assert(EGM_LUT_RELATIVE_MAX_UM == 24000,
    "runtime maximum relative position must match LUT");
_Static_assert(EGM_LUT_ENTRY_COUNT == 801U,
    "runtime LUT length must be 801 entries");
_Static_assert(EGM_PATH_LENGTH_UNITS % EGM_PROGRESS_STRIDE_UNITS == 0,
    "path progression must remain on the 60 um LUT grid");
_Static_assert(EGM_LUT_MAX_ABS_VALUE <= INT32_MAX / ICC_NETWORK_1D_PATH_COUNT,
    "four simultaneous EGM contributions can overflow int32_t");

static bool electrode_units_for_x(int32_t electrode_x_um, int16_t *units)
{
    if (units == NULL) {
        return false;
    }

    switch (electrode_x_um) {
    case 0:
        *units = 0;
        return true;
    case 6000:
        *units = 100;
        return true;
    case 12000:
        *units = 200;
        return true;
    case 18000:
        *units = 300;
        return true;
    case 24000:
        *units = 400;
        return true;
    default:
        return false;
    }
}

bool icc_egm_init(IccEgm *egm, int32_t electrode_x_um)
{
    if (egm == NULL) {
        return false;
    }
    egm->initialized = false;
    return icc_egm_set_electrode_x_um(egm, electrode_x_um);
}

bool icc_egm_set_electrode_x_um(IccEgm *egm, int32_t electrode_x_um)
{
    int16_t units;

    if (egm == NULL || !electrode_units_for_x(electrode_x_um, &units)) {
        return false;
    }
    egm->electrode_position_units = units;
    egm->initialized = true;
    return true;
}

int32_t icc_egm_electrode_x_um(const IccEgm *egm)
{
    if (egm == NULL || !egm->initialized) {
        return -1;
    }
    return (int32_t)egm->electrode_position_units *
        (int32_t)EGM_LUT_POSITION_STEP_UM;
}

static bool path_configuration_matches(
    const IccNetwork1d *network,
    uint8_t path_index)
{
    const IccPath *path = &network->paths[path_index];
    return path->initialized &&
        path->delay_ms == EGM_PATH_DELAY_MS &&
        path->gap_mm == EGM_PATH_GAP_MM &&
        path->cell_a == &network->cells[path_index] &&
        path->cell_b == &network->cells[path_index + 1U];
}

bool icc_egm_configuration_matches(const IccNetwork1d *network)
{
    if (network == NULL || !network->initialized) {
        return false;
    }

    for (uint8_t path_index = 0U;
         path_index < ICC_NETWORK_1D_PATH_COUNT;
         ++path_index) {
        if (!path_configuration_matches(network, path_index)) {
            return false;
        }
    }
    return true;
}

static bool path_contribution(
    const IccEgm *egm,
    const IccPath *path,
    uint8_t path_index,
    IccEgmValue *contribution)
{
    int32_t dipole_position_units;
    int32_t oriented_relative_units;
    int32_t lookup_index;

    *contribution = 0;
    switch (path->state) {
    case ICC_PATH_CELL_A_WAIT:
    case ICC_PATH_CELL_A_RELAY:
        if (path->progress_step >= EGM_PATH_STEP_COUNT) {
            return false;
        }
        dipole_position_units =
            (int32_t)path_index * EGM_PATH_LENGTH_UNITS +
            (int32_t)path->progress_step * EGM_PROGRESS_STRIDE_UNITS;
        oriented_relative_units =
            (int32_t)egm->electrode_position_units - dipole_position_units;
        break;

    case ICC_PATH_CELL_B_WAIT:
    case ICC_PATH_CELL_B_RELAY:
        if (path->progress_step >= EGM_PATH_STEP_COUNT) {
            return false;
        }
        dipole_position_units =
            (int32_t)(path_index + 1U) * EGM_PATH_LENGTH_UNITS -
            (int32_t)path->progress_step * EGM_PROGRESS_STRIDE_UNITS;
        oriented_relative_units =
            dipole_position_units - (int32_t)egm->electrode_position_units;
        break;

    case ICC_PATH_IDLE:
    case ICC_PATH_ANNIHILATE:
        return true;

    default:
        return false;
    }

    if (dipole_position_units < EGM_NETWORK_MIN_UNITS ||
        dipole_position_units > EGM_NETWORK_MAX_UNITS ||
        oriented_relative_units < EGM_RELATIVE_MIN_UNITS ||
        oriented_relative_units > EGM_RELATIVE_MAX_UNITS) {
        return false;
    }

    lookup_index = oriented_relative_units - EGM_RELATIVE_MIN_UNITS;
    if (lookup_index < 0 || lookup_index >= (int32_t)EGM_LUT_ENTRY_COUNT) {
        return false;
    }
    *contribution = kEgmRelativePotential[lookup_index];
    return true;
}

bool icc_egm_compute(
    const IccEgm *egm,
    const IccNetwork1d *network,
    IccEgmValue *result)
{
    IccEgmValue sum = 0;

    if (egm == NULL || !egm->initialized || result == NULL ||
        !icc_egm_configuration_matches(network)) {
        return false;
    }

    for (uint8_t path_index = 0U;
         path_index < ICC_NETWORK_1D_PATH_COUNT;
         ++path_index) {
        IccEgmValue contribution;
        if (!path_contribution(
                egm, &network->paths[path_index], path_index,
                &contribution)) {
            return false;
        }
        sum += contribution;
    }

    *result = sum;
    return true;
}
