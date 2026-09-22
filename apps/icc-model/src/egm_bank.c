#include "egm_bank.h"

#include <stddef.h>

static const int32_t kEgmElectrodePositionsUm[ICC_EGM_CHANNEL_COUNT] = {
    0,
    6000,
    12000,
    18000,
    24000
};

_Static_assert(ICC_EGM_CHANNEL_COUNT == 5U,
    "multi-electrode EGM requires the fixed five-cell network");

bool icc_egm_bank_init(IccEgmBank *bank)
{
    if (bank == NULL) {
        return false;
    }

    bank->initialized = false;
    for (uint8_t channel = 0U;
         channel < ICC_EGM_CHANNEL_COUNT;
         ++channel) {
        if (!icc_egm_init(
                &bank->channels[channel],
                kEgmElectrodePositionsUm[channel])) {
            return false;
        }
    }
    bank->initialized = true;
    return true;
}

bool icc_egm_bank_channel_for_x_um(
    const IccEgmBank *bank,
    int32_t electrode_x_um,
    uint8_t *channel_index)
{
    if (bank == NULL || !bank->initialized || channel_index == NULL) {
        return false;
    }

    for (uint8_t channel = 0U;
         channel < ICC_EGM_CHANNEL_COUNT;
         ++channel) {
        if (icc_egm_electrode_x_um(&bank->channels[channel]) ==
            electrode_x_um) {
            *channel_index = channel;
            return true;
        }
    }
    return false;
}

const IccEgm *icc_egm_bank_channel(
    const IccEgmBank *bank,
    uint8_t channel_index)
{
    if (bank == NULL || !bank->initialized ||
        channel_index >= ICC_EGM_CHANNEL_COUNT) {
        return NULL;
    }
    return &bank->channels[channel_index];
}

bool icc_egm_bank_compute(
    const IccEgmBank *bank,
    const IccNetwork1d *network,
    IccEgmValue results[ICC_EGM_CHANNEL_COUNT])
{
    IccEgmValue next_results[ICC_EGM_CHANNEL_COUNT];

    if (bank == NULL || !bank->initialized || network == NULL ||
        results == NULL) {
        return false;
    }

    for (uint8_t channel = 0U;
         channel < ICC_EGM_CHANNEL_COUNT;
         ++channel) {
        if (!icc_egm_compute(
                &bank->channels[channel],
                network,
                &next_results[channel])) {
            return false;
        }
    }
    for (uint8_t channel = 0U;
         channel < ICC_EGM_CHANNEL_COUNT;
         ++channel) {
        results[channel] = next_results[channel];
    }
    return true;
}
