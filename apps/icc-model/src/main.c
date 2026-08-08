#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <flexpret/csrs.h>
#include <flexpret/time.h>

#include "icc.h"
#include "network.h"
#include "path.h"

#ifndef ICC_PERIOD_NS
#define ICC_PERIOD_NS (ICC_TIMESTEP_MS * 1000000U)
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

#ifdef __EMULATOR__
#ifdef ICC_VERILATOR_TEST_SCENARIO
static void print_test_header(const IccNetwork1d *network)
{
    uint8_t index;

    printf("TEST,%d,timestep_ms,%u,period_ns,%u,path_delay_ms,%u,samples,%u\n",
           ICC_VERILATOR_TEST_SCENARIO,
           (unsigned)ICC_TIMESTEP_MS,
           (unsigned)ICC_PERIOD_NS,
           (unsigned)ICC_VERILATOR_TEST_PATH_DELAY_MS,
           (unsigned)ICC_VERILATOR_TEST_SAMPLES);
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
           "cell_0_state,cell_0_nv,cell_1_state,cell_1_nv,"
           "cell_2_state,cell_2_nv,cell_3_state,cell_3_nv,"
           "cell_4_state,cell_4_nv,path_0_state,path_1_state,"
           "path_2_state,path_3_state\n");
}

static void print_csv_row(
    uint32_t sample,
    uint32_t iteration_start,
    uint32_t measured_period,
    uint32_t release_lateness,
    const IccNetwork1d *network)
{
    printf("%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32 ",%" PRIu32
           ",%s,%" PRId32 ",%s,%" PRId32
           ",%s,%" PRId32 ",%s,%" PRId32
           ",%s,%" PRId32 ",%s,%s,%s,%s\n",
           sample,
           sample * ICC_TIMESTEP_MS,
           iteration_start,
           measured_period,
           release_lateness,
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
    uint32_t sample = 0U;
    uint32_t clock_probe_start;
    uint32_t clock_probe_end;
    uint32_t next_release;
    uint32_t previous_iteration_start;
#if defined(__EMULATOR__) && defined(ICC_VERILATOR_TEST_SCENARIO)
    IccState previous_cell_states[ICC_NETWORK_1D_CELL_COUNT];
    IccPathState previous_path_states[ICC_NETWORK_1D_PATH_COUNT];
    uint32_t minimum_measured_period = UINT32_MAX;
    uint32_t maximum_measured_period = 0U;
    uint32_t maximum_release_lateness = 0U;
    uint8_t index;
#endif

    if (!initialize_network(&network)) {
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

#ifdef __EMULATOR__
#ifdef ICC_VERILATOR_TEST_SCENARIO
    for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        previous_cell_states[index] = network.cells[index].state;
    }
    for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        previous_path_states[index] = network.paths[index].state;
    }
    print_test_header(&network);
#else
    print_csv_header();
#endif
#endif

    while (1) {
        uint32_t iteration_start;
        uint32_t measured_period;
        uint32_t release_lateness;
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
        for (index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
            previous_cell_states[index] = network.cells[index].state;
        }
        for (index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
            previous_path_states[index] = network.paths[index].state;
        }
        if (sample >= ICC_VERILATOR_TEST_SAMPLES) {
            printf("TIMING,%u,%u,%u\n",
                   (unsigned)minimum_measured_period,
                   (unsigned)maximum_measured_period,
                   (unsigned)maximum_release_lateness);
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
            &network);
#endif
#else
        (void)measured_period;
        (void)release_lateness;
#endif
    }
}
