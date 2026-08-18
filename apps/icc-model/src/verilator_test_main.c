/* Finite emulator validation harness; this file is never built for FPGA. */
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <flexpret/csrs.h>
#include <flexpret/time.h>

#include "egm.h"
#include "icc.h"
#include "network.h"
#include "path.h"

#ifndef ICC_PERIOD_NS
#define ICC_PERIOD_NS (ICC_TIMESTEP_MS * 1000000U)
#endif

#ifndef ICC_EGM_INITIAL_ELECTRODE_X_UM
#define ICC_EGM_INITIAL_ELECTRODE_X_UM ICC_EGM_DEFAULT_ELECTRODE_X_UM
#endif

#ifdef ICC_VERILATOR_TEST_SCENARIO
#ifndef ICC_VERILATOR_TEST_PATH_DELAY_MS
#define ICC_VERILATOR_TEST_PATH_DELAY_MS 1000U
#endif
#ifndef ICC_VERILATOR_TEST_SAMPLES
#define ICC_VERILATOR_TEST_SAMPLES 1000U
#endif
#endif

static bool initialize_network(IccNetwork1d *network)
{
#ifdef ICC_VERILATOR_TEST_SCENARIO
#if ICC_VERILATOR_TEST_SCENARIO == 0
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        20, 23, 26, 30, 40
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 1
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        23, 20, 26, 30, 40
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 2
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        30, 23, 20, 26, 40
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 3
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        40, 30, 26, 20, 23
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 4
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        40, 30, 26, 23, 20
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 5
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        20, 23, 26, 23, 20
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 6
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        30, 20, 26, 20, 30
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 7
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        20, 20, 20, 20, 20
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 8
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        23, 23, 23, 23, 23
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 9
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        26, 26, 26, 26, 26
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 10
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        30, 30, 30, 30, 30
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 11
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        40, 40, 40, 40, 40
    };
#elif ICC_VERILATOR_TEST_SCENARIO == 12 || \
      ICC_VERILATOR_TEST_SCENARIO == 13
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        0, 0, 0, 0, 0
    };
#else
#error "Unsupported ICC_VERILATOR_TEST_SCENARIO"
#endif
    static const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        ICC_VERILATOR_TEST_PATH_DELAY_MS,
        ICC_VERILATOR_TEST_PATH_DELAY_MS,
        ICC_VERILATOR_TEST_PATH_DELAY_MS,
        ICC_VERILATOR_TEST_PATH_DELAY_MS
    };
#else
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        0, 0, 0, 0, 20
    };
    static const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        1000U, 1000U, 1000U, 1000U
    };
#endif
    static const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {
        6U, 6U, 6U, 6U
    };

    return icc_network_1d_init(
        network,
        intervals,
        delays,
        gaps);
}

#if defined(ICC_VERILATOR_TEST_SCENARIO) && \
    (ICC_VERILATOR_TEST_SCENARIO == 12 || \
     ICC_VERILATOR_TEST_SCENARIO == 13)
static void initialize_egm_propagation_wave(IccNetwork1d *network)
{
    const uint8_t source_cell = ICC_VERILATOR_TEST_SCENARIO == 12 ? 0U : 4U;

    for (uint8_t cell = 0U; cell < ICC_NETWORK_1D_CELL_COUNT; ++cell) {
        network->cells[cell].state = ICC_Q0_RESTING;
        network->cells[cell].voltage_nv = -67633600;
        network->cells[cell].wait_ms_accum = 0U;
        network->cells[cell].relay = 0;
    }
    network->cells[source_cell].state = ICC_Q1_UPSTROKE;
    for (uint8_t path = 0U; path < ICC_NETWORK_1D_PATH_COUNT; ++path) {
        icc_path_step(&network->paths[path]);
    }
}
#endif

#ifdef __EMULATOR__
#ifdef ICC_VERILATOR_TEST_SCENARIO
static void print_test_header(const IccNetwork1d *network)
{
    uint8_t index;

    printf("TEST,%d,timestep_ms,%u,period_ns,%u,path_delay_ms,%u,samples,%u,"
           "electrode_x_um,%d\n",
           ICC_VERILATOR_TEST_SCENARIO,
           (unsigned)ICC_TIMESTEP_MS,
           (unsigned)ICC_PERIOD_NS,
           (unsigned)ICC_VERILATOR_TEST_PATH_DELAY_MS,
           (unsigned)ICC_VERILATOR_TEST_SAMPLES,
           (int)ICC_EGM_INITIAL_ELECTRODE_X_UM);
    for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        printf("CONFIG,%u,%d\n",
               (unsigned)index,
               (int)network->cells[index].pacemaker_interval_s);
    }
    printf("event,sample,time_ms,index,cause\n");
}

static void print_test_events(
    uint32_t sample,
    const IccNetwork1d *network,
    const IccState previous_cell_states[ICC_NETWORK_1D_CELL_COUNT],
    const IccPathState previous_path_states[ICC_NETWORK_1D_PATH_COUNT],
    const bool relay_before_step[ICC_NETWORK_1D_CELL_COUNT])
{
    uint8_t index;

    for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        if (network->cells[index].state == ICC_Q1_UPSTROKE &&
            previous_cell_states[index] != ICC_Q1_UPSTROKE) {
            printf("Q1,%u,%u,%u,%s\n",
                   (unsigned)sample,
                   (unsigned)(sample * ICC_TIMESTEP_MS),
                   (unsigned)index,
                   relay_before_step[index] ? "path" : "intrinsic");
        }
    }
    for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        if (network->paths[index].state == ICC_PATH_ANNIHILATE &&
            previous_path_states[index] != ICC_PATH_ANNIHILATE) {
            printf("ANNIHILATE,%u,%u,%u,path\n",
                   (unsigned)sample,
                   (unsigned)(sample * ICC_TIMESTEP_MS),
                   (unsigned)index);
        }
    }
}
#else
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
    int32_t electrode_x_um,
    const IccNetwork1d *network,
    IccEgmValue egm_value)
{
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
           electrode_x_um,
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
#endif
#endif

int main(void)
{
    IccNetwork1d network;
    IccEgm egm;
    uint32_t sample = 0U;
    uint32_t clock_probe_start;
    uint32_t clock_probe_end;
    uint32_t next_release;
    uint32_t previous_iteration_start;
    bool egm_configuration_matches;
#if defined(__EMULATOR__) && defined(ICC_VERILATOR_TEST_SCENARIO)
    IccState previous_cell_states[ICC_NETWORK_1D_CELL_COUNT];
    IccPathState previous_path_states[ICC_NETWORK_1D_PATH_COUNT];
    uint32_t minimum_measured_period = UINT32_MAX;
    uint32_t maximum_measured_period = 0U;
    uint32_t maximum_release_lateness = 0U;
    uint32_t maximum_execution_time = 0U;
    uint8_t index;
#endif

    if (!initialize_network(&network)) {
        return 1;
    }
#if defined(ICC_VERILATOR_TEST_SCENARIO) && \
    (ICC_VERILATOR_TEST_SCENARIO == 12 || \
     ICC_VERILATOR_TEST_SCENARIO == 13)
    initialize_egm_propagation_wave(&network);
#endif
    if (!icc_egm_init(&egm, ICC_EGM_INITIAL_ELECTRODE_X_UM)) {
        return 1;
    }
    egm_configuration_matches = icc_egm_configuration_matches(&network);
#ifndef ICC_VERILATOR_TEST_SCENARIO
    if (!egm_configuration_matches) {
        return 1;
    }
#endif

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

#ifdef __EMULATOR__
#ifdef ICC_VERILATOR_TEST_SCENARIO
    for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        previous_cell_states[index] = network.cells[index].state;
    }
    for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        previous_path_states[index] = network.paths[index].state;
    }
    print_test_header(&network);
#if ICC_VERILATOR_TEST_SCENARIO == 12
    printf("Q1,0,0,0,forced\n");
#elif ICC_VERILATOR_TEST_SCENARIO == 13
    printf("Q1,0,0,4,forced\n");
#endif
#else
    print_csv_header();
#endif
#endif

    while (1) {
        uint32_t iteration_start;
        uint32_t measured_period;
        uint32_t release_lateness;
#ifdef __EMULATOR__
        uint32_t iteration_end;
        uint32_t execution_time;
#endif
        IccEgmValue egm_value = 0;
#if defined(__EMULATOR__) && defined(ICC_VERILATOR_TEST_SCENARIO)
        bool relay_before_step[ICC_NETWORK_1D_CELL_COUNT];
#endif

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
#if defined(__EMULATOR__) && defined(ICC_VERILATOR_TEST_SCENARIO)
        if (measured_period < minimum_measured_period) {
            minimum_measured_period = measured_period;
        }
        if (measured_period > maximum_measured_period) {
            maximum_measured_period = measured_period;
        }
        if (release_lateness > maximum_release_lateness) {
            maximum_release_lateness = release_lateness;
        }
#endif
        previous_iteration_start = iteration_start;
        next_release += ICC_PERIOD_NS;

#if defined(__EMULATOR__) && defined(ICC_VERILATOR_TEST_SCENARIO)
        for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
            relay_before_step[index] = network.cells[index].relay != 0;
        }
#endif
        icc_network_1d_step(&network);
        if (egm_configuration_matches) {
            if (!icc_egm_compute(&egm, &network, &egm_value)) {
                return 1;
            }
        }
#ifdef __EMULATOR__
        iteration_end = rdtime();
        execution_time = iteration_end - iteration_start;
#if defined(__EMULATOR__) && defined(ICC_VERILATOR_TEST_SCENARIO)
        if (execution_time > maximum_execution_time) {
            maximum_execution_time = execution_time;
        }
#endif
#endif
        sample++;

#ifdef __EMULATOR__
#ifdef ICC_VERILATOR_TEST_SCENARIO
        (void)measured_period;
        (void)release_lateness;
        print_test_events(
            sample,
            &network,
            previous_cell_states,
            previous_path_states,
            relay_before_step);
#ifdef ICC_VERILATOR_EGM_TRACE
        printf("EGM,%u,%u,%d,%" PRId32 "\n",
               (unsigned)sample,
               (unsigned)(sample * ICC_TIMESTEP_MS),
               (int)icc_egm_electrode_x_um(&egm),
               egm_value);
#else
        (void)egm_value;
#endif
        for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
            previous_cell_states[index] = network.cells[index].state;
        }
        for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
            previous_path_states[index] = network.paths[index].state;
        }
        if (sample >= ICC_VERILATOR_TEST_SAMPLES) {
            printf("TIMING,%u,%u,%u,%u\n",
                   (unsigned)minimum_measured_period,
                   (unsigned)maximum_measured_period,
                   (unsigned)maximum_release_lateness,
                   (unsigned)maximum_execution_time);
            printf("DONE,%u,%u\n",
                   (unsigned)sample,
                   (unsigned)(sample * ICC_TIMESTEP_MS));
            return 0;
        }
#else
        print_csv_row(
            sample,
            iteration_start,
            measured_period,
            release_lateness,
            execution_time,
            icc_egm_electrode_x_um(&egm),
            &network,
            egm_value);
#endif
#else
        (void)measured_period;
        (void)release_lateness;
        (void)egm_value;
#endif
    }
}
