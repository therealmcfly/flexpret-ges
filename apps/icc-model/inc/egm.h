#ifndef ICC_MODEL_EGM_H
#define ICC_MODEL_EGM_H

#include <stdbool.h>
#include <stdint.h>

#include "network.h"

#define ICC_EGM_DEFAULT_ELECTRODE_X_UM 6000

typedef int32_t IccEgmValue;

typedef struct {
    int16_t electrode_position_units;
    bool initialized;
} IccEgm;

bool icc_egm_init(IccEgm *egm, int32_t electrode_x_um);
bool icc_egm_set_electrode_x_um(IccEgm *egm, int32_t electrode_x_um);
int32_t icc_egm_electrode_x_um(const IccEgm *egm);
bool icc_egm_configuration_matches(const IccNetwork1d *network);
bool icc_egm_compute(
    const IccEgm *egm,
    const IccNetwork1d *network,
    IccEgmValue *result);

#endif
