#include "icc.h"
#include "icc_calibration.h"

#include <stddef.h>

/* All voltage constants and increments are integer nanovolts. */
static const IccVoltageNv kResetVoltageNv = -67633600;
static const IccVoltageNv kQ0ToQ1Nv = -67633900;
static const IccVoltageNv kQ1ToQ2Nv = -24109100;
static const IccVoltageNv kQ2ToQ3Nv = -28989400;
static const IccVoltageNv kQ3ToQ0Nv = -66988400;
static const IccVoltageNv kVoltageFloorNv = -67000000;

/*
 * Scale the calibrated 200 ms increments using signed round-to-nearest integer
 * arithmetic. The expression is compile-time constant and introduces no
 * floating-point operation into the target binary.
 */
#define ICC_SCALE_200MS_INCREMENT(value) \
    ((IccVoltageNv)((((int64_t)(value) * (int64_t)ICC_TIMESTEP_MS) + \
        ((value) >= 0 ? 100 : -100)) / 200))

static const IccVoltageNv kQ1IncrementNv =
    ICC_SCALE_200MS_INCREMENT(8704960);
static const IccVoltageNv kQ2IncrementNv =
    ICC_SCALE_200MS_INCREMENT(-181952);
static const IccVoltageNv kQ3IncrementNv =
    ICC_SCALE_200MS_INCREMENT(-1727227);


bool icc_interval_is_supported(int8_t interval_s)
{
    switch (interval_s) {
    case -1:
    case 0:
    case 15:
    case 20:
    case 23:
    case 26:
    case 30:
    case 40:
        return true;
    default:
        return false;
    }
}

void icc_init(Icc *cell, int8_t pacemaker_interval_s)
{
    if (cell == NULL) {
        return;
    }

    cell->state = ICC_WAIT;
    cell->voltage_nv = 0;
    cell->wait_ms_accum = 0U;
    cell->pacemaker_interval_s = pacemaker_interval_s;
    cell->relay = 0;
    cell->initialized = true;
}

IccVoltageNv icc_step(Icc *cell)
{
    if (cell == NULL || !cell->initialized) {
        return 0;
    }

    switch (cell->state) {
    case ICC_Q0_RESTING:
        if (cell->relay > 0 || cell->voltage_nv < kQ0ToQ1Nv) {
            if (cell->pacemaker_interval_s == -1) {
                cell->relay = 0;
                break;
            }
            cell->state = ICC_Q1_UPSTROKE;
            cell->relay = 0;
        }
        break;

    case ICC_Q1_UPSTROKE:
        cell->relay = 0;
        if (cell->voltage_nv >= kQ1ToQ2Nv) {
            cell->state = ICC_Q2_PLATEAU;
        }
        break;

    case ICC_Q2_PLATEAU:
        cell->relay = 0;
        if (cell->voltage_nv < kQ2ToQ3Nv) {
            cell->state = ICC_Q3_REPOLARIZATION;
        }
        break;

    case ICC_Q3_REPOLARIZATION:
        cell->relay = 0;
        if (cell->voltage_nv < kQ3ToQ0Nv) {
            cell->state = ICC_Q0_RESTING;
        }
        break;

    case ICC_WAIT:
    default:
        cell->wait_ms_accum += ICC_TIMESTEP_MS;
        if (cell->wait_ms_accum >= ICC_WAIT_MS) {
            cell->voltage_nv = kResetVoltageNv;
            cell->state = ICC_Q0_RESTING;
            cell->wait_ms_accum = 0U;
        }
        break;
    }

    switch (cell->state) {
    case ICC_Q0_RESTING:
        cell->voltage_nv += icc_calibrated_resting_increment_nv(
            cell->pacemaker_interval_s);
        break;

    case ICC_Q1_UPSTROKE:
        cell->voltage_nv += kQ1IncrementNv;
        if (cell->voltage_nv >= kQ1ToQ2Nv) {
            cell->voltage_nv = kQ1ToQ2Nv;
        }
        break;

    case ICC_Q2_PLATEAU:
        cell->voltage_nv += kQ2IncrementNv;
        break;

    case ICC_Q3_REPOLARIZATION:
        cell->voltage_nv += kQ3IncrementNv;
        if (cell->voltage_nv < kVoltageFloorNv) {
            cell->voltage_nv = kVoltageFloorNv;
        }
        break;

    case ICC_WAIT:
    default:
        break;
    }

    return cell->voltage_nv;
}

bool icc_is_active(const Icc *cell)
{
    return cell != NULL &&
           cell->initialized &&
           (cell->state == ICC_Q1_UPSTROKE ||
            cell->state == ICC_Q2_PLATEAU ||
            cell->state == ICC_Q3_REPOLARIZATION);
}

void icc_stimulate(Icc *cell)
{
    if (cell == NULL || !cell->initialized) {
        return;
    }

    cell->relay = 1;
}

int32_t icc_voltage_nearest_uv(const Icc *cell)
{
    int32_t quotient;
    int32_t remainder;

    if (cell == NULL) {
        return 0;
    }

    quotient = cell->voltage_nv / ICC_NV_PER_UV;
    remainder = cell->voltage_nv % ICC_NV_PER_UV;

    /* Round to nearest microvolt, with exact halves toward positive infinity. */
    if (remainder >= ICC_NV_PER_UV / 2) {
        quotient += 1;
    } else if (remainder < -(ICC_NV_PER_UV / 2)) {
        quotient -= 1;
    }

    return quotient;
}

const char *icc_state_name(IccState state)
{
    switch (state) {
    case ICC_Q0_RESTING:
        return "Q0";
    case ICC_Q1_UPSTROKE:
        return "Q1";
    case ICC_Q2_PLATEAU:
        return "Q2";
    case ICC_Q3_REPOLARIZATION:
        return "Q3";
    case ICC_WAIT:
        return "WAIT";
    default:
        return "UNKNOWN";
    }
}
