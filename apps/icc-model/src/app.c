#include "app.h"

#include <stddef.h>

bool icc_model_app_init(
    IccModelApp *app,
    const int8_t cell_intervals_s[ICC_NETWORK_1D_CELL_COUNT],
    const uint16_t path_delays_ms[ICC_NETWORK_1D_PATH_COUNT],
    const uint8_t path_gaps_mm[ICC_NETWORK_1D_PATH_COUNT],
    int32_t electrode_x_um)
{
    if (app == NULL) {
        return false;
    }

    app->initialized = false;
    if (!icc_network_1d_init(
            &app->network,
            cell_intervals_s,
            path_delays_ms,
            path_gaps_mm)) {
        return false;
    }
    if (!icc_egm_init(&app->egm, electrode_x_um)) {
        return false;
    }
    if (!icc_egm_configuration_matches(&app->network)) {
        return false;
    }
    app->initialized = true;
    return true;
}

bool icc_model_app_step(IccModelApp *app, IccEgmValue *egm_value)
{
    if (app == NULL || egm_value == NULL || !app->initialized) {
        return false;
    }

    icc_network_1d_step(&app->network);
    return icc_egm_compute(&app->egm, &app->network, egm_value);
}
