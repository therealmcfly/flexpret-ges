#ifndef ICC_MODEL_EGM_H
#define ICC_MODEL_EGM_H

#include <stdbool.h>
#include <stdint.h>

#include "network.h"

typedef int32_t IccEgmValue;

bool icc_egm_1d5c_configuration_matches(const IccNetwork1d *network);
IccEgmValue icc_egm_1d5c_compute(const IccNetwork1d *network);

#endif
