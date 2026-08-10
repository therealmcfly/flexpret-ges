#ifndef ICC_MODEL_EGM_LUT_SELECT_H
#define ICC_MODEL_EGM_LUT_SELECT_H

#if ICC_TIMESTEP_MS == 200U
#include "egm_lut_200ms.h"
#elif ICC_TIMESTEP_MS == 100U
#include "egm_lut_100ms.h"
#elif ICC_TIMESTEP_MS == 50U
#include "egm_lut_50ms.h"
#elif ICC_TIMESTEP_MS == 20U
#include "egm_lut_20ms.h"
#elif ICC_TIMESTEP_MS == 10U
#include "egm_lut_10ms.h"
#else
#error "No EGM lookup table exists for ICC_TIMESTEP_MS"
#endif

#endif
