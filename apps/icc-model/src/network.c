#include "network.h"

#include <stddef.h>

bool icc_network_1d_init(
    IccNetwork1d *network,
    const int8_t cell_intervals_s[ICC_NETWORK_1D_CELL_COUNT],
    const uint16_t path_delays_ms[ICC_NETWORK_1D_PATH_COUNT],
    const uint8_t path_gaps_mm[ICC_NETWORK_1D_PATH_COUNT])
{
    uint8_t index;

    if (network == NULL || cell_intervals_s == NULL ||
        path_delays_ms == NULL || path_gaps_mm == NULL) {
        return false;
    }

    network->initialized = false;

    for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        if (!icc_interval_is_supported(cell_intervals_s[index])) {
            return false;
        }
    }

    for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        if (path_delays_ms[index] <= ICC_TIMESTEP_MS ||
            path_delays_ms[index] % ICC_TIMESTEP_MS != 0U ||
            path_gaps_mm[index] == 0U) {
            return false;
        }
    }

    for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        icc_init(&network->cells[index], cell_intervals_s[index]);
    }

    for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        icc_path_init(
            &network->paths[index],
            &network->cells[index],
            &network->cells[index + 1U],
            path_delays_ms[index],
            path_gaps_mm[index]);
    }

    network->initialized = true;
    return true;
}

void icc_network_1d_step(IccNetwork1d *network)
{
    uint8_t index;

    if (network == NULL || !network->initialized) {
        return;
    }

    /* Preserve iccnet-core ordering: every cell updates before every path. */
    for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        (void)icc_step(&network->cells[index]);
    }

    for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        icc_path_step(&network->paths[index]);
    }
}
