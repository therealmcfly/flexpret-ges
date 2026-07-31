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
    uint32_t next_release = rdtime() + ICC_PERIOD_NS;

    icc_init(&cell, ICC_INTERVAL_SECONDS);

    printf("ICC integer-nanovolt single-cell model\n");
    printf("sample,time_ms,state,voltage_nv,nearest_uv\n");

    while (1) {
        fp_delay_until(next_release);
        next_release += ICC_PERIOD_NS;

        (void)icc_step(&cell);
        sample++;

        printf("%" PRIu32 ",%" PRIu32 ",%s,%" PRId32 ",%" PRId32 "\n",
               sample,
               sample * ICC_TIMESTEP_MS,
               icc_state_name(cell.state),
               cell.voltage_nv,
               icc_voltage_nearest_uv(&cell));
    }
}
