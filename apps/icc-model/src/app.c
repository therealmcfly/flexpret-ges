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
    if (!icc_egm_bank_init(&app->egm_bank)) {
        return false;
    }
    if (!icc_egm_configuration_matches(&app->network)) {
        return false;
    }
    if (!icc_egm_bank_channel_for_x_um(
            &app->egm_bank,
            electrode_x_um,
            &app->selected_egm_channel_index)) {
        return false;
    }
    app->pacing_lead_cell_index = app->selected_egm_channel_index;
    app->initialized = true;
    return true;
}

bool icc_model_app_step(IccModelApp *app, IccEgmValue *egm_value)
{
    const IccEgm *selected_channel;

    if (app == NULL || egm_value == NULL || !app->initialized) {
        return false;
    }

    selected_channel = icc_egm_bank_channel(
        &app->egm_bank,
        app->selected_egm_channel_index);
    if (selected_channel == NULL) {
        return false;
    }

    icc_network_1d_step(&app->network);
    return icc_egm_compute(selected_channel, &app->network, egm_value);
}

bool icc_model_app_step_all(
    IccModelApp *app,
    IccEgmValue egm_values[ICC_EGM_CHANNEL_COUNT])
{
    if (app == NULL || egm_values == NULL || !app->initialized) {
        return false;
    }

    icc_network_1d_step(&app->network);
    return icc_egm_bank_compute(
        &app->egm_bank,
        &app->network,
        egm_values);
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
