#include <stdint.h>

#include <flexpret/csrs.h>
#include <flexpret/time.h>
#include <flexpret/uart.h>

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
        0, 0, 0, 0, 23
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

static void send_egm_uart2(IccEgmValue egm_value)
{
    int32_t scaled_value = egm_value >> 12;
    uint16_t raw_value;

    if (scaled_value > INT16_MAX) {
        scaled_value = INT16_MAX;
    } else if (scaled_value < INT16_MIN) {
        scaled_value = INT16_MIN;
    }
    raw_value = (uint16_t)(int16_t)scaled_value;

    uart_send(UART2_BASE, UINT8_C(0xAA));
    uart_send(UART2_BASE, UINT8_C(0x55));
    uart_send(UART2_BASE, (uint8_t)(raw_value & UINT16_C(0xFF)));
    uart_send(UART2_BASE, (uint8_t)(raw_value >> 8));
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
        fp_nop;
        fp_nop;
        fp_nop;
        fp_nop;
        next_release += ICC_PERIOD_NS;
        if (!icc_model_app_step(&app, &egm_value)) {
            return 1;
        }
        send_egm_uart2(egm_value);
    }
}
