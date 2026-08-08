#ifndef ICC_MODEL_CALIBRATION_H
#define ICC_MODEL_CALIBRATION_H

#include "icc.h"

/*
 * Best constant integer Q0 increments found by exhaustive search against the
 * ICC state machine. Values are nanovolts per ICC_TIMESTEP_MS model step.
 */
#if ICC_TIMESTEP_MS == 200U
#define ICC_RESTING_15S_NV (-32003)
#define ICC_RESTING_20S_NV (-14307)
#define ICC_RESTING_23S_NV (-10593)
#define ICC_RESTING_26S_NV (-8489)
#define ICC_RESTING_30S_NV (-6728)
#define ICC_RESTING_40S_NV (-4401)
#elif ICC_TIMESTEP_MS == 100U
#define ICC_RESTING_15S_NV (-15847)
#define ICC_RESTING_20S_NV (-7043)
#define ICC_RESTING_23S_NV (-5282)
#define ICC_RESTING_26S_NV (-4226)
#define ICC_RESTING_30S_NV (-3336)
#define ICC_RESTING_40S_NV (-2185)
#elif ICC_TIMESTEP_MS == 50U
#define ICC_RESTING_15S_NV (-7730)
#define ICC_RESTING_20S_NV (-3482)
#define ICC_RESTING_23S_NV (-2619)
#define ICC_RESTING_26S_NV (-2099)
#define ICC_RESTING_30S_NV (-1659)
#define ICC_RESTING_40S_NV (-1089)
#elif ICC_TIMESTEP_MS == 20U
#define ICC_RESTING_15S_NV (-3033)
#define ICC_RESTING_20S_NV (-1381)
#define ICC_RESTING_23S_NV (-1040)
#define ICC_RESTING_26S_NV (-835)
#define ICC_RESTING_30S_NV (-661)
#define ICC_RESTING_40S_NV (-434)
#elif ICC_TIMESTEP_MS == 10U
#define ICC_RESTING_15S_NV (-1520)
#define ICC_RESTING_20S_NV (-695)
#define ICC_RESTING_23S_NV (-524)
#define ICC_RESTING_26S_NV (-421)
#define ICC_RESTING_30S_NV (-333)
#define ICC_RESTING_40S_NV (-219)
#else
#error "No ICC resting-increment calibration exists for this timestep"
#endif

static inline IccVoltageNv
icc_calibrated_resting_increment_nv(int8_t interval_s)
{
    switch (interval_s) {
    case 15:
        return ICC_RESTING_15S_NV;
    case 20:
        return ICC_RESTING_20S_NV;
    case 23:
        return ICC_RESTING_23S_NV;
    case 26:
        return ICC_RESTING_26S_NV;
    case 30:
        return ICC_RESTING_30S_NV;
    case 40:
        return ICC_RESTING_40S_NV;
    case -1: /* Blocked cell. */
    case 0:  /* Follower cell. */
    default:
        return 0;
    }
}

#endif
