#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#include "egm.h"
#include "egm_relative_lut.h"

#if ICC_TIMESTEP_MS == 10U
#define TEST_STRIDE_UNITS 1
#define EXPECTED_MINIMUM_OFFSET_MS 110
#elif ICC_TIMESTEP_MS == 20U
#define TEST_STRIDE_UNITS 2
#define EXPECTED_MINIMUM_OFFSET_MS 100
#elif ICC_TIMESTEP_MS == 50U
#define TEST_STRIDE_UNITS 5
#define EXPECTED_MINIMUM_OFFSET_MS 100
#elif ICC_TIMESTEP_MS == 100U
#define TEST_STRIDE_UNITS 10
#define EXPECTED_MINIMUM_OFFSET_MS 100
#elif ICC_TIMESTEP_MS == 200U
#define TEST_STRIDE_UNITS 20
#define EXPECTED_MINIMUM_OFFSET_MS 200
#else
#error "unsupported test timestep"
#endif

#define TEST_STEP_COUNT (100 / TEST_STRIDE_UNITS)

static const int32_t kElectrodePositionsUm[ICC_NETWORK_1D_CELL_COUNT] = {
    0, 6000, 12000, 18000, 24000
};

static void configure_network(IccNetwork1d *network)
{
    const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {0, 0, 0, 0, 0};
    const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        1000U, 1000U, 1000U, 1000U
    };
    const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {6U, 6U, 6U, 6U};
    assert(icc_network_1d_init(network, intervals, delays, gaps));
}

static int32_t expected_entry(
    uint8_t electrode_cell,
    uint8_t path_index,
    bool a_to_b,
    uint16_t progress_step)
{
    const int32_t electrode_units = (int32_t)electrode_cell * 100;
    int32_t dipole_units;
    int32_t oriented_units;

    if (a_to_b) {
        dipole_units = (int32_t)path_index * 100 +
            (int32_t)progress_step * TEST_STRIDE_UNITS;
        oriented_units = electrode_units - dipole_units;
    } else {
        dipole_units = (int32_t)(path_index + 1U) * 100 -
            (int32_t)progress_step * TEST_STRIDE_UNITS;
        oriented_units = dipole_units - electrode_units;
    }
    assert(oriented_units >= -400 && oriented_units <= 400);
    return kEgmRelativePotential[oriented_units + 400];
}

static void test_electrode_api(void)
{
    IccEgm egm;

    assert(!icc_egm_init(NULL, 0));
    assert(!icc_egm_init(&egm, -60));
    assert(!egm.initialized);
    assert(!icc_egm_set_electrode_x_um(&egm, 60));
    assert(!icc_egm_set_electrode_x_um(&egm, 24060));

    for (uint8_t repetition = 0U; repetition < 3U; ++repetition) {
        for (uint8_t cell = 0U; cell < ICC_NETWORK_1D_CELL_COUNT; ++cell) {
            assert(icc_egm_set_electrode_x_um(
                &egm, kElectrodePositionsUm[cell]));
            assert(icc_egm_electrode_x_um(&egm) ==
                kElectrodePositionsUm[cell]);
        }
    }
    assert(icc_egm_set_electrode_x_um(&egm, 12000));
    assert(!icc_egm_set_electrode_x_um(&egm, 60));
    assert(icc_egm_electrode_x_um(&egm) == 12000);
    assert(icc_egm_electrode_x_um(NULL) == -1);
}

static void test_all_positions_paths_directions_and_steps(void)
{
    IccNetwork1d network;
    IccEgm egm;
    IccEgmValue result;

    configure_network(&network);
    assert(icc_egm_configuration_matches(&network));

    for (uint8_t electrode_cell = 0U;
         electrode_cell < ICC_NETWORK_1D_CELL_COUNT;
         ++electrode_cell) {
        assert(icc_egm_init(&egm, kElectrodePositionsUm[electrode_cell]));

        for (uint8_t path_index = 0U;
             path_index < ICC_NETWORK_1D_PATH_COUNT;
             ++path_index) {
            for (uint8_t direction = 0U; direction < 2U; ++direction) {
                for (uint16_t step = 0U; step < TEST_STEP_COUNT; ++step) {
                    network.paths[path_index].state = direction == 0U
                        ? ICC_PATH_CELL_A_WAIT : ICC_PATH_CELL_B_WAIT;
                    network.paths[path_index].progress_step = step;
                    assert(icc_egm_compute(&egm, &network, &result));
                    assert(result == expected_entry(
                        electrode_cell, path_index, direction == 0U, step));

                    if (step == 0U || step + 1U == TEST_STEP_COUNT) {
                        network.paths[path_index].state = direction == 0U
                            ? ICC_PATH_CELL_A_RELAY : ICC_PATH_CELL_B_RELAY;
                        assert(icc_egm_compute(&egm, &network, &result));
                        assert(result == expected_entry(
                            electrode_cell, path_index, direction == 0U,
                            step));
                    }
                    network.paths[path_index].state = ICC_PATH_IDLE;
                    network.paths[path_index].progress_step = 0U;
                }
            }
        }
    }
}

static void test_sum_and_invalid_configurations(void)
{
    IccNetwork1d network;
    IccEgm egm;
    IccEgm uninitialized_egm = {0};
    IccEgmValue result;
    int32_t expected = 0;

    configure_network(&network);
    assert(icc_egm_init(&egm, 12000));
    assert(icc_egm_compute(&egm, &network, &result));
    assert(result == 0);

    for (uint8_t path_index = 0U;
         path_index < ICC_NETWORK_1D_PATH_COUNT;
         ++path_index) {
        network.paths[path_index].state = path_index % 2U == 0U
            ? ICC_PATH_CELL_A_WAIT : ICC_PATH_CELL_B_WAIT;
        network.paths[path_index].progress_step = path_index;
        expected += expected_entry(
            2U, path_index, path_index % 2U == 0U, path_index);
    }
    assert(icc_egm_compute(&egm, &network, &result));
    assert(result == expected);

    assert(!icc_egm_compute(NULL, &network, &result));
    assert(!icc_egm_compute(&uninitialized_egm, &network, &result));
    assert(!icc_egm_compute(&egm, NULL, &result));
    assert(!icc_egm_compute(&egm, &network, NULL));

    network.paths[0].progress_step = TEST_STEP_COUNT;
    assert(!icc_egm_compute(&egm, &network, &result));
    network.paths[0].progress_step = 0U;
    network.paths[0].delay_ms = 2000U;
    assert(!icc_egm_configuration_matches(&network));
    assert(!icc_egm_compute(&egm, &network, &result));
    network.paths[0].delay_ms = 1000U;
    network.paths[1].gap_mm = 5U;
    assert(!icc_egm_configuration_matches(&network));
    network.paths[1].gap_mm = 6U;
    network.paths[2].cell_a = &network.cells[0];
    assert(!icc_egm_configuration_matches(&network));
}

static void set_resting(Icc *cell)
{
    cell->state = ICC_Q0_RESTING;
    cell->voltage_nv = -67633600;
    cell->wait_ms_accum = 0U;
    cell->relay = 0;
}

static void test_alignment_for_electrode(uint8_t electrode_cell)
{
    IccNetwork1d network;
    IccEgm egm;
    IccEgmValue value;
    const bool a_to_b = electrode_cell < 4U;
    const uint8_t source_cell = a_to_b ? 0U : 4U;
    int32_t q1_time_ms = electrode_cell == source_cell ? 0 : -1;
    int32_t minimum_time_ms = -1;
    int32_t minimum_value = INT32_MAX;
    IccState previous_state;

    configure_network(&network);
    for (uint8_t cell = 0U; cell < ICC_NETWORK_1D_CELL_COUNT; ++cell) {
        set_resting(&network.cells[cell]);
    }
    network.cells[source_cell].state = ICC_Q1_UPSTROKE;
    for (uint8_t path = 0U; path < ICC_NETWORK_1D_PATH_COUNT; ++path) {
        icc_path_step(&network.paths[path]);
    }
    assert(icc_egm_init(&egm, kElectrodePositionsUm[electrode_cell]));
    previous_state = network.cells[electrode_cell].state;

    assert(icc_egm_compute(&egm, &network, &value));
    if (q1_time_ms == 0) {
        minimum_value = value;
        minimum_time_ms = 0;
    }

    for (int32_t time_ms = (int32_t)ICC_TIMESTEP_MS;
         time_ms <= 5000;
         time_ms += (int32_t)ICC_TIMESTEP_MS) {
        icc_network_1d_step(&network);
        assert(icc_egm_compute(&egm, &network, &value));

        if (q1_time_ms < 0 &&
            network.cells[electrode_cell].state == ICC_Q1_UPSTROKE &&
            previous_state != ICC_Q1_UPSTROKE) {
            q1_time_ms = time_ms;
            minimum_value = value;
            minimum_time_ms = time_ms;
        }
        if (q1_time_ms >= 0 && time_ms < q1_time_ms + 1000 &&
            value < minimum_value) {
            minimum_value = value;
            minimum_time_ms = time_ms;
        }
        previous_state = network.cells[electrode_cell].state;
    }

    printf("ALIGNMENT,%u,%d,%u,%d,%d,%d,%d,%u,%s\n",
        (unsigned)(electrode_cell + 1U),
        kElectrodePositionsUm[electrode_cell],
        (unsigned)(electrode_cell + 1U),
        q1_time_ms,
        minimum_time_ms,
        minimum_time_ms - q1_time_ms,
        minimum_value,
        (unsigned)ICC_TIMESTEP_MS,
        a_to_b ? "A_TO_B" : "B_TO_A");
    fflush(stdout);
    assert(q1_time_ms >= 0);
    assert(minimum_value < 0);
    assert(minimum_time_ms >= q1_time_ms);
    assert(minimum_time_ms - q1_time_ms == EXPECTED_MINIMUM_OFFSET_MS);
}

int main(void)
{
    test_electrode_api();
    test_all_positions_paths_directions_and_steps();
    test_sum_and_invalid_configurations();
    for (uint8_t electrode = 0U;
         electrode < ICC_NETWORK_1D_CELL_COUNT;
         ++electrode) {
        test_alignment_for_electrode(electrode);
    }
    printf("RUNTIME,timestep_ms,%u,electrodes,5,paths,4,directions,2,"
           "progress_steps,%u\n",
        (unsigned)ICC_TIMESTEP_MS, (unsigned)TEST_STEP_COUNT);
    puts("EGM relative runtime validation passed");
    return 0;
}
