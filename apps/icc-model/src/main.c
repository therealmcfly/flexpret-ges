#include <stdint.h>

#include <flexpret/csrs.h>
#include <flexpret/time.h>

#include "app.h"

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

int main(void)
{
    IccModelApp app;
    IccEgmValue egm_value;
    uint32_t clock_probe_start;
    uint32_t clock_probe_end;
    uint32_t next_release;

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
    while (1) {
        fp_delay_until(next_release);
        next_release += ICC_PERIOD_NS;
        if (!icc_model_app_step(&app, &egm_value)) {
            return 1;
        }
    }
}
