#ifndef ICC_MODEL_EGM_BANK_H
#define ICC_MODEL_EGM_BANK_H

#include <stdbool.h>
#include <stdint.h>

#include "egm.h"

#define ICC_EGM_CHANNEL_COUNT ICC_NETWORK_1D_CELL_COUNT

typedef struct {
    IccEgm channels[ICC_EGM_CHANNEL_COUNT];
    bool initialized;
} IccEgmBank;

bool icc_egm_bank_init(IccEgmBank *bank);
bool icc_egm_bank_channel_for_x_um(
    const IccEgmBank *bank,
    int32_t electrode_x_um,
    uint8_t *channel_index);
const IccEgm *icc_egm_bank_channel(
    const IccEgmBank *bank,
    uint8_t channel_index);
bool icc_egm_bank_compute(
    const IccEgmBank *bank,
    const IccNetwork1d *network,
    IccEgmValue results[ICC_EGM_CHANNEL_COUNT]);

#endif
