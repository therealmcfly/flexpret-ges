#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "egm_relative_lut.h"

#define REFERENCE_DIPOLE_MOMENT 18.0
#define REFERENCE_LONGITUDINAL_WEIGHT 1.0
#define REFERENCE_TRANSVERSE_WEIGHT 0.1
#define ABSOLUTE_ERROR_TOLERANCE 5.00001e-8
#define MEANINGFUL_REFERENCE_THRESHOLD 1.0e-5

static double independent_reference(int32_t position_um)
{
    const double x = (double)position_um * 0.001;
    const double h = (double)EGM_LUT_ELECTRODE_HEIGHT_UM * 0.001;
    const double squared_radius = x * x + h * h;
    const double inverse_distance_cubed =
        1.0 / (squared_radius * sqrt(squared_radius));

    return REFERENCE_DIPOLE_MOMENT *
        (REFERENCE_LONGITUDINAL_WEIGHT * x -
         REFERENCE_TRANSVERSE_WEIGHT * h) *
        inverse_distance_cubed;
}

static uint32_t index_for_position(int32_t position_um)
{
    assert(position_um >= EGM_LUT_RELATIVE_MIN_UM);
    assert(position_um <= EGM_LUT_RELATIVE_MAX_UM);
    assert((position_um - EGM_LUT_RELATIVE_MIN_UM) %
        (int32_t)EGM_LUT_POSITION_STEP_UM == 0);
    return (uint32_t)((position_um - EGM_LUT_RELATIVE_MIN_UM) /
        (int32_t)EGM_LUT_POSITION_STEP_UM);
}

int main(void)
{
    double maximum_absolute_error = 0.0;
    double maximum_meaningful_relative_error = 0.0;
    int32_t worst_absolute_position_um = 0;
    int32_t worst_relative_position_um = 0;

    _Static_assert(EGM_LUT_RELATIVE_MIN_UM == -24000,
        "unexpected minimum relative position");
    _Static_assert(EGM_LUT_RELATIVE_MAX_UM == 24000,
        "unexpected maximum relative position");
    _Static_assert(EGM_LUT_POSITION_STEP_UM == 60U,
        "unexpected relative-position step");
    _Static_assert(EGM_LUT_ENTRY_COUNT == 801U,
        "unexpected relative table length");
    _Static_assert(EGM_LUT_TABLE_BYTES == 3204U,
        "unexpected relative table byte count");
    _Static_assert(sizeof(kEgmRelativePotential) == 3204U,
        "lookup array storage is not 3204 bytes");
    _Static_assert(EGM_LUT_MAX_ABS_VALUE <= INT32_MAX / 4,
        "four active dipoles can overflow int32_t");

    for (uint32_t index = 0U; index < EGM_LUT_ENTRY_COUNT; ++index) {
        const int32_t position_um = EGM_LUT_RELATIVE_MIN_UM +
            (int32_t)index * (int32_t)EGM_LUT_POSITION_STEP_UM;
        const double reference = independent_reference(position_um);
        const double reconstructed =
            (double)kEgmRelativePotential[index] / (double)EGM_LUT_SCALE;
        const double absolute_error = fabs(reconstructed - reference);

        assert(absolute_error <= ABSOLUTE_ERROR_TOLERANCE);
        if (absolute_error > maximum_absolute_error) {
            maximum_absolute_error = absolute_error;
            worst_absolute_position_um = position_um;
        }
        if (fabs(reference) >= MEANINGFUL_REFERENCE_THRESHOLD) {
            const double relative_error = absolute_error / fabs(reference);
            if (relative_error > maximum_meaningful_relative_error) {
                maximum_meaningful_relative_error = relative_error;
                worst_relative_position_um = position_um;
            }
        }
    }

    const uint32_t minimum_index = index_for_position(-24000);
    const uint32_t negative_near_index = index_for_position(-600);
    const uint32_t zero_index = index_for_position(0);
    const uint32_t positive_near_index = index_for_position(600);
    const uint32_t maximum_index = index_for_position(24000);

    assert(minimum_index == 0U);
    assert(maximum_index == EGM_LUT_ENTRY_COUNT - 1U);
    assert(kEgmRelativePotential[negative_near_index] < 0);
    assert(kEgmRelativePotential[positive_near_index] > 0);
    assert(kEgmRelativePotential[zero_index] == -18000000);
    assert(kEgmRelativePotential[negative_near_index] <
        kEgmRelativePotential[zero_index]);

    /* The transverse term is symmetric and negative, so the complete table
     * is not antisymmetric. Check the correct pair identity instead. */
    for (int32_t position_um = 60;
         position_um <= EGM_LUT_RELATIVE_MAX_UM;
         position_um += (int32_t)EGM_LUT_POSITION_STEP_UM) {
        const double pair =
            (double)kEgmRelativePotential[index_for_position(position_um)] /
                (double)EGM_LUT_SCALE +
            (double)kEgmRelativePotential[index_for_position(-position_um)] /
                (double)EGM_LUT_SCALE;
        const double reference_pair =
            independent_reference(position_um) +
            independent_reference(-position_um);
        assert(fabs(pair - reference_pair) <=
            2.0 * ABSOLUTE_ERROR_TOLERANCE);
    }

    printf("NUMERICAL,entries,%u,bytes,%u,scale,%d,max_abs_error,%.17g,"
           "worst_abs_position_um,%d,max_meaningful_relative_error,%.17g,"
           "worst_relative_position_um,%d,tolerance,%.17g\n",
        EGM_LUT_ENTRY_COUNT, EGM_LUT_TABLE_BYTES, EGM_LUT_SCALE,
        maximum_absolute_error, worst_absolute_position_um,
        maximum_meaningful_relative_error, worst_relative_position_um,
        ABSOLUTE_ERROR_TOLERANCE);
    puts("EGM relative lookup numerical validation passed");
    return EXIT_SUCCESS;
}
