#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "egm_bank.h"

static const int8_t kIntervals[ICC_NETWORK_1D_CELL_COUNT] = {
    0, 0, 0, 0, 0
};
static const uint16_t kDelays[ICC_NETWORK_1D_PATH_COUNT] = {
    1000U, 1000U, 1000U, 1000U
};
static const uint8_t kGaps[ICC_NETWORK_1D_PATH_COUNT] = {
    6U, 6U, 6U, 6U
};

static void initialize_network(IccNetwork1d *network)
{
    assert(icc_network_1d_init(
        network,
        kIntervals,
        kDelays,
        kGaps));
}

static void assert_bank_matches_independent_channels(
    const IccEgmBank *bank,
    const IccNetwork1d *network)
{
    IccEgmValue bank_values[ICC_EGM_CHANNEL_COUNT];
    IccEgmValue independent_value;

    assert(icc_egm_bank_compute(bank, network, bank_values));
    for (uint8_t channel = 0U;
         channel < ICC_EGM_CHANNEL_COUNT;
         ++channel) {
        IccEgm independent;
        assert(icc_egm_init(&independent, (int32_t)channel * 6000));
        assert(icc_egm_compute(
            &independent,
            network,
            &independent_value));
        assert(bank_values[channel] == independent_value);
    }
}

static void test_channel_initialization_and_lookup(void)
{
    IccEgmBank bank;
    uint8_t channel = UINT8_MAX;

    assert(icc_egm_bank_init(&bank));
    assert(bank.initialized);
    for (uint8_t expected = 0U;
         expected < ICC_EGM_CHANNEL_COUNT;
         ++expected) {
        const IccEgm *egm = icc_egm_bank_channel(&bank, expected);
        assert(egm != NULL);
        assert(icc_egm_electrode_x_um(egm) == (int32_t)expected * 6000);
        assert(icc_egm_bank_channel_for_x_um(
            &bank,
            (int32_t)expected * 6000,
            &channel));
        assert(channel == expected);
    }

    assert(!icc_egm_bank_channel_for_x_um(&bank, -1, &channel));
    assert(!icc_egm_bank_channel_for_x_um(&bank, 60, &channel));
    assert(!icc_egm_bank_channel_for_x_um(&bank, 24060, &channel));
    assert(icc_egm_bank_channel(&bank, ICC_EGM_CHANNEL_COUNT) == NULL);
}

static void test_all_paths_directions_and_progress(void)
{
    const uint16_t progression_steps = 1000U / ICC_TIMESTEP_MS;
    IccNetwork1d network;
    IccEgmBank bank;

    initialize_network(&network);
    assert(icc_egm_bank_init(&bank));

    for (uint8_t path = 0U;
         path < ICC_NETWORK_1D_PATH_COUNT;
         ++path) {
        for (uint8_t direction = 0U; direction < 2U; ++direction) {
            for (uint16_t progress = 0U;
                 progress < progression_steps;
                 ++progress) {
                for (uint8_t index = 0U;
                     index < ICC_NETWORK_1D_PATH_COUNT;
                     ++index) {
                    network.paths[index].state = ICC_PATH_IDLE;
                    network.paths[index].progress_step = 0U;
                }
                network.paths[path].state =
                    direction == 0U
                        ? ICC_PATH_CELL_A_WAIT
                        : ICC_PATH_CELL_B_WAIT;
                network.paths[path].progress_step = progress;
                assert_bank_matches_independent_channels(&bank, &network);
                assert(network.paths[path].progress_step == progress);
                assert(network.paths[path].state ==
                    (direction == 0U
                        ? ICC_PATH_CELL_A_WAIT
                        : ICC_PATH_CELL_B_WAIT));
            }
        }
    }
}

static void test_atomic_failure_and_invalid_inputs(void)
{
    IccNetwork1d network;
    IccEgmBank bank;
    IccEgmBank uninitialized = {0};
    IccEgmValue results[ICC_EGM_CHANNEL_COUNT] = {
        11, 22, 33, 44, 55
    };

    initialize_network(&network);
    assert(icc_egm_bank_init(&bank));

    network.paths[0].delay_ms = 2000U;
    assert(!icc_egm_bank_compute(&bank, &network, results));
    assert(results[0] == 11);
    assert(results[1] == 22);
    assert(results[2] == 33);
    assert(results[3] == 44);
    assert(results[4] == 55);

    assert(!icc_egm_bank_init(NULL));
    assert(!icc_egm_bank_channel_for_x_um(NULL, 0, NULL));
    assert(icc_egm_bank_channel(NULL, 0U) == NULL);
    assert(!icc_egm_bank_compute(NULL, &network, results));
    assert(!icc_egm_bank_compute(&uninitialized, &network, results));
    assert(!icc_egm_bank_compute(&bank, NULL, results));
    assert(!icc_egm_bank_compute(&bank, &network, NULL));
}

int main(void)
{
    test_channel_initialization_and_lookup();
    test_all_paths_directions_and_progress();
    test_atomic_failure_and_invalid_inputs();
    printf("EGM multi-electrode bank tests passed\n");
    return 0;
}
