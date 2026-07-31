#ifndef ICC_MODEL_ICC_H
#define ICC_MODEL_ICC_H

#include <stdbool.h>
#include <stdint.h>

#define ICC_TIMESTEP_MS 200U
#define ICC_WAIT_MS 4999U

#define ICC_NV_PER_UV 1000
#define ICC_NV_PER_MV 1000000

/* ICC voltage stored as a signed integer number of nanovolts. */
typedef int32_t IccVoltageNv;

typedef enum {
    ICC_Q0_RESTING = 0,
    ICC_Q1_UPSTROKE = 1,
    ICC_Q2_PLATEAU = 2,
    ICC_Q3_REPOLARIZATION = 3,
    ICC_WAIT = 4
} IccState;

typedef struct {
    IccState state;
    IccVoltageNv voltage_nv;
    uint32_t wait_ms_accum;
    int8_t pacemaker_interval_s;
    int8_t relay;
    bool initialized;
} Icc;

void icc_init(Icc *cell, int8_t pacemaker_interval_s);
IccVoltageNv icc_step(Icc *cell);
int32_t icc_voltage_nearest_uv(const Icc *cell);
const char *icc_state_name(IccState state);

#endif
