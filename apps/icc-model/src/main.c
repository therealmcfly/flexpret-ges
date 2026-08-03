#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <flexpret/csrs.h>
#include <flexpret/time.h>

#include "icc.h"

#define ICC_PERIOD_NS 200000000U
#define ICC_INTERVAL_SECONDS 20

int main(void)
{
    Icc cell;
    uint32_t sample = 0U;
    uint32_t clock_probe_start = rdtime();
    uint32_t clock_probe_end;
    uint32_t next_release;
    uint32_t previous_iteration_start;

    /* Allow the FlexPRET pipeline to retire rdtime() before using its value. */
    fp_nop;
    fp_nop;
    fp_nop;
    fp_nop;
    clock_probe_end = rdtime();

    if (clock_probe_end == clock_probe_start) {
        printf("ERROR: rdtime did not advance during startup.\n");
        return 1;
    }

    next_release = clock_probe_end + ICC_PERIOD_NS;
    previous_iteration_start = clock_probe_end;

    icc_init(&cell, ICC_INTERVAL_SECONDS);

    printf("ICC integer-nanovolt single-cell model\n");
    printf("sample,time_ms,fpga_time_ns,period_ns,release_lateness_ns,"
           "state,voltage_nv,nearest_uv\n");

    while (1) {
        uint32_t iteration_start;
        uint32_t measured_period;
        uint32_t release_lateness;

        fp_delay_until(next_release);
        /* Separate delay completion from rdtime() to avoid a pipeline hazard. */
        fp_nop;
        fp_nop;
        fp_nop;
        fp_nop;
        iteration_start = rdtime();
        /* Allow rdtime() to retire before consuming iteration_start. */
        fp_nop;
        fp_nop;
        fp_nop;
        fp_nop;
        measured_period = iteration_start - previous_iteration_start;
        release_lateness = iteration_start - next_release;
        previous_iteration_start = iteration_start;
        next_release += ICC_PERIOD_NS;

        (void)icc_step(&cell);
        sample++;

        printf("%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
               ",%s,%" PRId32 ",%" PRId32 "\n",
               sample,
               sample * ICC_TIMESTEP_MS,
               iteration_start,
               measured_period,
               release_lateness,
               icc_state_name(cell.state),
               cell.voltage_nv,
               icc_voltage_nearest_uv(&cell));
    }
}
