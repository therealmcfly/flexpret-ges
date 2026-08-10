#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "egm.h"
#include "egm_lut_select.h"

static void configure_network(IccNetwork1d *network)
{
    memset(network, 0, sizeof(*network));
    network->initialized = true;
    for (uint8_t index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        network->paths[index].initialized = true;
        network->paths[index].delay_ms = EGM_LUT_DELAY_MS;
        network->paths[index].gap_mm = EGM_LUT_GAP_MM;
        network->paths[index].state = ICC_PATH_IDLE;
    }
}

static void test_real_path_progression(uint8_t direction)
{
    IccNetwork1d network;
    IccPath *path;
    Icc *source;
    uint8_t table_direction;

    configure_network(&network);
    icc_init(&network.cells[0], 0);
    icc_init(&network.cells[1], 0);
    path = &network.paths[0];
    icc_path_init(
        path,
        &network.cells[0],
        &network.cells[1],
        EGM_LUT_DELAY_MS,
        EGM_LUT_GAP_MM);

    if (direction == EGM_LUT_DIRECTION_A_TO_B) {
        source = &network.cells[0];
        table_direction = EGM_LUT_DIRECTION_A_TO_B;
    } else {
        source = &network.cells[1];
        table_direction = EGM_LUT_DIRECTION_B_TO_A;
    }

    source->state = ICC_Q1_UPSTROKE;
    icc_path_step(path);
    assert(path->progress_step == 0U);
    assert(icc_egm_1d5c_compute(&network) ==
        kEgmPathLookup[0][table_direction][0]);

    source->state = ICC_Q2_PLATEAU;
    for (uint16_t step = 1U; step < EGM_LUT_STEP_COUNT; ++step) {
        icc_path_step(path);
        assert(path->progress_step == step);
        assert(icc_egm_1d5c_compute(&network) ==
            kEgmPathLookup[0][table_direction][step]);
    }

    icc_path_step(path);
    assert(path->state == ICC_PATH_IDLE);
    assert(path->progress_step == 0U);
    assert(icc_egm_1d5c_compute(&network) == 0);
}

int main(void)
{
    IccNetwork1d network;
    configure_network(&network);

    assert(icc_egm_1d5c_configuration_matches(&network));
    assert(icc_egm_1d5c_compute(&network) == 0);

    network.paths[0].state = ICC_PATH_CELL_A_WAIT;
    network.paths[0].progress_step = 0U;
    assert(icc_egm_1d5c_compute(&network) ==
        kEgmPathLookup[0][EGM_LUT_DIRECTION_A_TO_B][0]);

    network.paths[1].state = ICC_PATH_CELL_B_RELAY;
    network.paths[1].progress_step = EGM_LUT_STEP_COUNT - 1U;
    assert(icc_egm_1d5c_compute(&network) ==
        kEgmPathLookup[0][EGM_LUT_DIRECTION_A_TO_B][0] +
        kEgmPathLookup[1][EGM_LUT_DIRECTION_B_TO_A]
            [EGM_LUT_STEP_COUNT - 1U]);

    network.paths[2].state = ICC_PATH_ANNIHILATE;
    network.paths[2].progress_step = 1U;
    assert(icc_egm_1d5c_compute(&network) ==
        kEgmPathLookup[0][EGM_LUT_DIRECTION_A_TO_B][0] +
        kEgmPathLookup[1][EGM_LUT_DIRECTION_B_TO_A]
            [EGM_LUT_STEP_COUNT - 1U]);

    network.paths[3].state = ICC_PATH_CELL_A_WAIT;
    network.paths[3].progress_step = EGM_LUT_STEP_COUNT;
    assert(icc_egm_1d5c_compute(&network) ==
        kEgmPathLookup[0][EGM_LUT_DIRECTION_A_TO_B][0] +
        kEgmPathLookup[1][EGM_LUT_DIRECTION_B_TO_A]
            [EGM_LUT_STEP_COUNT - 1U]);

    network.paths[3].delay_ms++;
    assert(!icc_egm_1d5c_configuration_matches(&network));

    test_real_path_progression(EGM_LUT_DIRECTION_A_TO_B);
    test_real_path_progression(EGM_LUT_DIRECTION_B_TO_A);

    puts("EGM runtime lookup-and-sum test passed");
    return 0;
}
