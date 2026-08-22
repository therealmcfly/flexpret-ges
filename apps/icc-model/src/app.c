#include "app.h"

#include <stddef.h>

static bool find_cell_at_position(
    int32_t electrode_x_um,
    uint8_t *cell_index)
{
    static const int32_t cell_positions_um[ICC_NETWORK_1D_CELL_COUNT] = {
        0, 6000, 12000, 18000, 24000
    };

    if (cell_index == NULL) {
        return false;
    }

    for (uint8_t cell = 0U; cell < ICC_NETWORK_1D_CELL_COUNT; ++cell) {
        if (cell_positions_um[cell] == electrode_x_um) {
            *cell_index = cell;
            return true;
        }
    }
    return false;
}

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
    if (!find_cell_at_position(
            electrode_x_um,
            &app->pacing_lead_cell_index)) {
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

bool icc_model_app_apply_pacing(IccModelApp *app)
{
    if (app == NULL ||
        !app->initialized ||
        app->pacing_lead_cell_index >= ICC_NETWORK_1D_CELL_COUNT) {
        return false;
    }

    icc_stimulate(&app->network.cells[app->pacing_lead_cell_index]);
    return true;
}
