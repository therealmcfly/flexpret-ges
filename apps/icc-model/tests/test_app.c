#include <assert.h>
#include <stdio.h>

#include "app.h"

static const int8_t kIntervals[ICC_NETWORK_1D_CELL_COUNT] = {
    0, 0, 0, 0, 20
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
    assert(icc_model_app_step(&app, &egm_value));
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
}

int main(void)
{
    test_valid_application();
    test_invalid_configuration();
    test_invalid_step();
    printf("ICC application integration tests passed\n");
    return 0;
}
