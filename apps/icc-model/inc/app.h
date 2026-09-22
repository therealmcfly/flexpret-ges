#ifndef ICC_MODEL_APP_H
#define ICC_MODEL_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "egm_bank.h"
#include "network.h"

typedef struct {
    IccNetwork1d network;
    IccEgmBank egm_bank;
    uint8_t selected_egm_channel_index;
    uint8_t pacing_lead_cell_index;
    bool initialized;
} IccModelApp;

bool icc_model_app_init(
    IccModelApp *app,
    const int8_t cell_intervals_s[ICC_NETWORK_1D_CELL_COUNT],
    const uint16_t path_delays_ms[ICC_NETWORK_1D_PATH_COUNT],
    const uint8_t path_gaps_mm[ICC_NETWORK_1D_PATH_COUNT],
    int32_t electrode_x_um);

bool icc_model_app_step(IccModelApp *app, IccEgmValue *egm_value);
bool icc_model_app_step_all(
    IccModelApp *app,
    IccEgmValue egm_values[ICC_EGM_CHANNEL_COUNT]);
bool icc_model_app_apply_pacing(IccModelApp *app);

#endif
