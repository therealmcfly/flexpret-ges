#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>

#include <flexpret/csrs.h>
#include <flexpret/time.h>

#include "app.h"
#include "icc.h"
#include "path.h"

#ifndef ICC_PERIOD_NS
#define ICC_PERIOD_NS (ICC_TIMESTEP_MS * 1000000U)
#endif

#ifndef ICC_EGM_INITIAL_ELECTRODE_X_UM
#define ICC_EGM_INITIAL_ELECTRODE_X_UM ICC_EGM_DEFAULT_ELECTRODE_X_UM
#endif

static bool initialize_app(IccModelApp *app)
{
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        0, 0, 0, 0, 20
    };
    static const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        1000U, 1000U, 1000U, 1000U
    };
    static const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {
        6U, 6U, 6U, 6U
    };

    return icc_model_app_init(
        app,
        intervals,
        delays,
        gaps,
        ICC_EGM_INITIAL_ELECTRODE_X_UM);
}

static void print_csv_header(void)
{
    printf("ICC integer-nanovolt five-cell 1D network\n");
    printf("sample,time_ms,fpga_time_ns,period_ns,release_lateness_ns,"
           "execution_time_ns,egm_electrode_x_um,"
           "cell_0_state,cell_0_nv,cell_1_state,cell_1_nv,"
           "cell_2_state,cell_2_nv,cell_3_state,cell_3_nv,"
           "cell_4_state,cell_4_nv,egm_scaled,path_0_state,path_1_state,"
           "path_2_state,path_3_state\n");
}

static void print_csv_row(
    uint32_t sample,
    uint32_t iteration_start,
    uint32_t measured_period,
    uint32_t release_lateness,
    uint32_t execution_time,
    const IccModelApp *app,
    IccEgmValue egm_value)
{
    const IccNetwork1d *network = &app->network;

    printf("%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
           ",%" PRIu32 ",%" PRId32
           ",%s,%" PRId32 ",%s,%" PRId32
           ",%s,%" PRId32 ",%s,%" PRId32
           ",%s,%" PRId32 ",%" PRId32 ",%s,%s,%s,%s\n",
           sample,
           sample * ICC_TIMESTEP_MS,
           iteration_start,
           measured_period,
           release_lateness,
           execution_time,
           icc_egm_electrode_x_um(&app->egm),
           icc_state_name(network->cells[0].state),
           network->cells[0].voltage_nv,
           icc_state_name(network->cells[1].state),
           network->cells[1].voltage_nv,
           icc_state_name(network->cells[2].state),
           network->cells[2].voltage_nv,
           icc_state_name(network->cells[3].state),
           network->cells[3].voltage_nv,
           icc_state_name(network->cells[4].state),
           network->cells[4].voltage_nv,
           egm_value,
           icc_path_state_name(network->paths[0].state),
           icc_path_state_name(network->paths[1].state),
           icc_path_state_name(network->paths[2].state),
           icc_path_state_name(network->paths[3].state));
}

int main(void)
{
    IccModelApp app;
    uint32_t sample = 0U;
    uint32_t clock_probe_start;
    uint32_t clock_probe_end;
    uint32_t next_release;
    uint32_t previous_iteration_start;

    if (!initialize_app(&app)) {
        return 1;
    }

    clock_probe_start = rdtime();
    fp_nop;
    fp_nop;
    fp_nop;
    fp_nop;
    clock_probe_end = rdtime();
    fp_nop;
    fp_nop;
    fp_nop;
    fp_nop;
    if (clock_probe_end == clock_probe_start) {
        return 1;
    }

    next_release = clock_probe_end + ICC_PERIOD_NS;
    previous_iteration_start = clock_probe_end;
    print_csv_header();

    while (1) {
        uint32_t iteration_start;
        uint32_t iteration_end;
        uint32_t measured_period;
        uint32_t release_lateness;
        uint32_t execution_time;
        IccEgmValue egm_value;

        fp_delay_until(next_release);
        fp_nop;
        fp_nop;
        fp_nop;
        fp_nop;
        iteration_start = rdtime();
        fp_nop;
        fp_nop;
        fp_nop;
        fp_nop;
        measured_period = iteration_start - previous_iteration_start;
        release_lateness = iteration_start - next_release;
        previous_iteration_start = iteration_start;
        next_release += ICC_PERIOD_NS;

        if (!icc_model_app_step(&app, &egm_value)) {
            return 1;
        }
        iteration_end = rdtime();
        execution_time = iteration_end - iteration_start;
        sample++;

        print_csv_row(
            sample,
            iteration_start,
            measured_period,
            release_lateness,
            execution_time,
            &app,
            egm_value);
    }
}
