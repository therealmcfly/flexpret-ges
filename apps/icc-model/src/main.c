#include <stdint.h>

#include <flexpret/csrs.h>
#include <flexpret/io.h>
#include <flexpret/time.h>
#include <flexpret/uart.h>
#include <flexpret/wb.h>

#include "app.h"

#ifndef ICC_PERIOD_NS
#define ICC_PERIOD_NS (ICC_TIMESTEP_MS * 1000000U)
#endif

#ifndef ICC_EGM_INITIAL_ELECTRODE_X_UM
#define ICC_EGM_INITIAL_ELECTRODE_X_UM ICC_EGM_DEFAULT_ELECTRODE_X_UM
#endif

#ifndef ICC_CELL1_INTERVAL_S
#define ICC_CELL1_INTERVAL_S 20
#define ICC_CELL2_INTERVAL_S 0
#define ICC_CELL3_INTERVAL_S 0
#define ICC_CELL4_INTERVAL_S 0
#define ICC_CELL5_INTERVAL_S 0
#endif

#ifndef ICC_PATH1_DELAY_MS
#define ICC_PATH1_DELAY_MS 1000U
#define ICC_PATH2_DELAY_MS 1000U
#define ICC_PATH3_DELAY_MS 1000U
#define ICC_PATH4_DELAY_MS 1000U
#endif

#ifndef ICC_PATH1_GAP_MM
#define ICC_PATH1_GAP_MM 6U
#define ICC_PATH2_GAP_MM 6U
#define ICC_PATH3_GAP_MM 6U
#define ICC_PATH4_GAP_MM 6U
#endif

#ifndef ICC_MODEL_UART
#define ICC_MODEL_UART 2
#endif

#if ICC_MODEL_UART == 1
#define ICC_MODEL_UART_BASE UART1_BASE
#elif ICC_MODEL_UART == 2
#define ICC_MODEL_UART_BASE UART2_BASE
#else
#error "ICC_MODEL_UART must be 1 or 2"
#endif

static bool initialize_app(IccModelApp *app)
{
    static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        ICC_CELL1_INTERVAL_S,
        ICC_CELL2_INTERVAL_S,
        ICC_CELL3_INTERVAL_S,
        ICC_CELL4_INTERVAL_S,
        ICC_CELL5_INTERVAL_S
    };
    static const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        ICC_PATH1_DELAY_MS,
        ICC_PATH2_DELAY_MS,
        ICC_PATH3_DELAY_MS,
        ICC_PATH4_DELAY_MS
    };
    static const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {
        ICC_PATH1_GAP_MM,
        ICC_PATH2_GAP_MM,
        ICC_PATH3_GAP_MM,
        ICC_PATH4_GAP_MM
    };

    return icc_model_app_init(
        app,
        intervals,
        delays,
        gaps,
        ICC_EGM_INITIAL_ELECTRODE_X_UM);
}

static void print_configuration(const IccModelApp *app)
{
    fp_print_string(
        "--------------- ICC Model on FlexPRET Start ---------------\n");
    printf("Timestep:             %u ms\n", (unsigned)ICC_TIMESTEP_MS);
    printf("Cell intervals:       [%d, %d, %d, %d, %d] s\n",
        (int)ICC_CELL1_INTERVAL_S,
        (int)ICC_CELL2_INTERVAL_S,
        (int)ICC_CELL3_INTERVAL_S,
        (int)ICC_CELL4_INTERVAL_S,
        (int)ICC_CELL5_INTERVAL_S);
    printf("Path delays:          [%u, %u, %u, %u] ms\n",
        (unsigned)ICC_PATH1_DELAY_MS,
        (unsigned)ICC_PATH2_DELAY_MS,
        (unsigned)ICC_PATH3_DELAY_MS,
        (unsigned)ICC_PATH4_DELAY_MS);
    printf("Path gaps:            [%u, %u, %u, %u] mm\n",
        (unsigned)ICC_PATH1_GAP_MM,
        (unsigned)ICC_PATH2_GAP_MM,
        (unsigned)ICC_PATH3_GAP_MM,
        (unsigned)ICC_PATH4_GAP_MM);
    printf("Electrode position:   %d um\n",
        (int)ICC_EGM_INITIAL_ELECTRODE_X_UM);
    printf("Electrode cell:       Cell %u\n",
        (unsigned)app->pacing_lead_cell_index + 1U);
    printf("Pacing-lead cell:     Cell %u\n",
        (unsigned)app->pacing_lead_cell_index + 1U);
    printf("EGM/pacing UART:      UART%u\n", (unsigned)ICC_MODEL_UART);
    fp_print_string("EGM frame:            AA 55 + little-endian int16\n");
    fp_print_string("ICC Model Start!\n");
}

static int16_t send_egm_uart(IccEgmValue egm_value)
{
    int32_t scaled_value = egm_value >> 12;
    uint16_t raw_value;

    if (scaled_value > INT16_MAX) {
        scaled_value = INT16_MAX;
    } else if (scaled_value < INT16_MIN) {
        scaled_value = INT16_MIN;
    }
    raw_value = (uint16_t)(int16_t)scaled_value;

    uart_send(ICC_MODEL_UART_BASE, UINT8_C(0xAA));
    uart_send(ICC_MODEL_UART_BASE, UINT8_C(0x55));
    uart_send(ICC_MODEL_UART_BASE,
        (uint8_t)(raw_value & UINT16_C(0xFF)));
    uart_send(ICC_MODEL_UART_BASE, (uint8_t)(raw_value >> 8));
    return (int16_t)scaled_value;
}

static bool check_pacing_uart(
    IccModelApp *app,
    uint32_t step,
    uint8_t *pacing_value)
{
    uint32_t status = wb_read(ICC_MODEL_UART_BASE + UART_CSR_OFF);
    uint8_t cell_index;
    const Icc *cell;

    if (pacing_value == NULL) {
        return false;
    }
    *pacing_value = 0U;

    if (!UART_DATA_READY(status)) {
        return true;
    }
    *pacing_value = uart_receive(ICC_MODEL_UART_BASE);
    if (*pacing_value == 0U) {
        return true;
    }

    cell_index = app->pacing_lead_cell_index;
    if (cell_index >= ICC_NETWORK_1D_CELL_COUNT) {
        return false;
    }
    cell = &app->network.cells[cell_index];
    printf("PACE_RX: step=%u value=%u cell=%u state=%s relay=%d\n",
        (unsigned)step,
        (unsigned)*pacing_value,
        (unsigned)cell_index + 1U,
        icc_state_name(cell->state),
        (int)cell->relay);
    return icc_model_app_apply_pacing(app);
}

int main(void)
{
    IccModelApp app;
    IccEgmValue egm_value;
    const Icc *paced_cell;
    int16_t transmitted_egm;
    uint32_t clock_probe_start;
    uint32_t clock_probe_end;
    uint32_t next_release;
    uint32_t step = 0U;
    uint8_t pacing_value;

    if (!initialize_app(&app)) {
        return 1;
    }
    print_configuration(&app);

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
        if (!check_pacing_uart(&app, step, &pacing_value)) {
            return 1;
        }
        if (!icc_model_app_step(&app, &egm_value)) {
            return 1;
        }
        transmitted_egm = send_egm_uart(egm_value);
        if (pacing_value != 0U) {
            paced_cell = &app.network.cells[app.pacing_lead_cell_index];
            printf("PACE_RESULT: step=%u value=%u cell=%u state=%s egm=%d\n",
                (unsigned)step,
                (unsigned)pacing_value,
                (unsigned)app.pacing_lead_cell_index + 1U,
                icc_state_name(paced_cell->state),
                (int)transmitted_egm);
        }
        step++;
    }
}
