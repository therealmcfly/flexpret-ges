#include <assert.h>
#include <stdio.h>

#include "app.h"

static const int8_t kIntervals[ICC_NETWORK_1D_CELL_COUNT] = {
    0, 0, 0, 0, 20
};
static const int8_t kNoIntrinsicIntervals[ICC_NETWORK_1D_CELL_COUNT] = {
    0, 0, 0, 0, 0
};
static const uint16_t kDelays[ICC_NETWORK_1D_PATH_COUNT] = {
    1000U, 1000U, 1000U, 1000U
};
static const uint8_t kGaps[ICC_NETWORK_1D_PATH_COUNT] = {
    6U, 6U, 6U, 6U
};

static void test_valid_application(void)
{
    IccModelApp app;
    IccEgmValue egm_value;

    assert(icc_model_app_init(
        &app, kIntervals, kDelays, kGaps, 6000));
    assert(app.initialized);
    assert(app.pacing_lead_cell_index == 1U);
    assert(icc_model_app_step(&app, &egm_value));
}

static void test_pacing_lead_tracks_electrode(void)
{
    static const int32_t electrode_positions_um[ICC_NETWORK_1D_CELL_COUNT] = {
        0, 6000, 12000, 18000, 24000
    };
    IccModelApp app;

    for (uint8_t cell = 0U; cell < ICC_NETWORK_1D_CELL_COUNT; ++cell) {
        assert(icc_model_app_init(
            &app,
            kIntervals,
            kDelays,
            kGaps,
            electrode_positions_um[cell]));
        assert(app.pacing_lead_cell_index == cell);
        assert(icc_model_app_apply_pacing(&app));

        for (uint8_t index = 0U;
             index < ICC_NETWORK_1D_CELL_COUNT;
             ++index) {
            assert(app.network.cells[index].relay ==
                (index == cell ? 1 : 0));
        }
    }
}

static void test_pacing_after_ten_seconds(void)
{
    static const int32_t electrode_positions_um[ICC_NETWORK_1D_CELL_COUNT] = {
        0, 6000, 12000, 18000, 24000
    };
    const uint32_t steps_before_pacing = 10000U / ICC_TIMESTEP_MS;
    IccModelApp app;
    IccEgmValue egm_value;

    for (uint8_t paced_cell = 0U;
         paced_cell < ICC_NETWORK_1D_CELL_COUNT;
         ++paced_cell) {
        assert(icc_model_app_init(
            &app,
            kNoIntrinsicIntervals,
            kDelays,
            kGaps,
            electrode_positions_um[paced_cell]));

        for (uint32_t step = 0U; step < steps_before_pacing; ++step) {
            assert(icc_model_app_step(&app, &egm_value));
            for (uint8_t cell = 0U;
                 cell < ICC_NETWORK_1D_CELL_COUNT;
                 ++cell) {
                assert(!icc_is_active(&app.network.cells[cell]));
            }
        }

        assert(icc_model_app_apply_pacing(&app));
        assert(icc_model_app_step(&app, &egm_value));

        for (uint8_t cell = 0U;
             cell < ICC_NETWORK_1D_CELL_COUNT;
             ++cell) {
            if (cell == paced_cell) {
                assert(app.network.cells[cell].state == ICC_Q1_UPSTROKE);
                assert(icc_is_active(&app.network.cells[cell]));
            } else {
                assert(!icc_is_active(&app.network.cells[cell]));
            }
        }
    }
}

static void test_invalid_configuration(void)
{
    IccModelApp app;
    uint16_t wrong_delays[ICC_NETWORK_1D_PATH_COUNT] = {
        2000U, 2000U, 2000U, 2000U
    };

    assert(!icc_model_app_init(
        NULL, kIntervals, kDelays, kGaps, 6000));
    assert(!icc_model_app_init(
        &app, kIntervals, kDelays, kGaps, 1234));
    assert(!icc_model_app_init(
        &app, kIntervals, wrong_delays, kGaps, 6000));
}

static void test_invalid_step(void)
{
    IccModelApp app = {0};
    IccEgmValue egm_value;

    assert(!icc_model_app_step(NULL, &egm_value));
    assert(!icc_model_app_step(&app, NULL));
    assert(!icc_model_app_step(&app, &egm_value));
    assert(!icc_model_app_apply_pacing(NULL));
    assert(!icc_model_app_apply_pacing(&app));
}

int main(void)
{
    test_valid_application();
    test_invalid_configuration();
    test_invalid_step();
    test_pacing_lead_tracks_electrode();
    test_pacing_after_ten_seconds();
    printf("ICC application integration tests passed\n");
    return 0;
}
