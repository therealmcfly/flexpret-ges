#include "rispa_frame.h"

#include <stddef.h>
_Static_assert(ICC_NETWORK_1D_CELL_COUNT == 5U,
    "RiSPA voltage frame requires exactly five ICC cells");

bool icc_rispa_encode_voltage_frame(
    const int32_t voltage_nv[ICC_NETWORK_1D_CELL_COUNT],
    uint8_t frame[ICC_RISPA_FRAME_SIZE])
{
    if (voltage_nv == NULL || frame == NULL) {
        return false;
    }
    frame[0] = ICC_RISPA_SYNC0;
    frame[1] = ICC_RISPA_SYNC1;
    for (uint8_t cell = 0U; cell < ICC_NETWORK_1D_CELL_COUNT; ++cell) {
        uint32_t raw = (uint32_t)voltage_nv[cell];
        uint8_t offset = (uint8_t)(2U + (cell << 2));

        frame[offset] = (uint8_t)raw;
        frame[offset + 1U] = (uint8_t)(raw >> 8);
        frame[offset + 2U] = (uint8_t)(raw >> 16);
        frame[offset + 3U] = (uint8_t)(raw >> 24);
    }
    return true;
}
