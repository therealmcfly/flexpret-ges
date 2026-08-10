#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

#ifndef EGM_LUT_HEADER
#error "EGM_LUT_HEADER must name one generated lookup header"
#endif

#ifndef EXPECTED_EGM_TIMESTEP_MS
#error "EXPECTED_EGM_TIMESTEP_MS must be defined"
#endif

#ifndef EXPECTED_EGM_STEP_COUNT
#error "EXPECTED_EGM_STEP_COUNT must be defined"
#endif

#include EGM_LUT_HEADER

_Static_assert(EGM_LUT_TIMESTEP_MS == EXPECTED_EGM_TIMESTEP_MS,
    "lookup timestep metadata mismatch");
_Static_assert(EGM_LUT_STEP_COUNT == EXPECTED_EGM_STEP_COUNT,
    "lookup step-count metadata mismatch");
_Static_assert(EGM_LUT_DELAY_MS % EGM_LUT_TIMESTEP_MS == 0U,
    "path delay must be divisible by timestep");
_Static_assert(EGM_LUT_STEP_COUNT ==
    EGM_LUT_DELAY_MS / EGM_LUT_TIMESTEP_MS,
    "step count must match delay divided by timestep");
_Static_assert(EGM_LUT_SCALE == 10000000,
    "all supported timesteps must use one common EGM scale");

int main(void)
{
    int64_t maximum_magnitude = 0;

    for (uint32_t direction = 0U; direction < 2U; ++direction) {
        for (uint32_t step = 0U; step < EGM_LUT_STEP_COUNT; ++step) {
            int64_t value = kEgmPath0Lookup[direction][step];
            if (value < 0) {
                value = -value;
            }
            if (value > maximum_magnitude) {
                maximum_magnitude = value;
            }
        }
    }

    assert(maximum_magnitude > 0);
    assert(maximum_magnitude * 4 < INT32_MAX);
    puts("EGM lookup metadata and range test passed");
    return 0;
}
