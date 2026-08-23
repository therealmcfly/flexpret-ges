#ifndef ICC_MODEL_RISPA_FRAME_H
#define ICC_MODEL_RISPA_FRAME_H

#include <stdbool.h>
#include <stdint.h>

#include "network.h"

#define ICC_RISPA_SYNC0 UINT8_C(0xAA)
#define ICC_RISPA_SYNC1 UINT8_C(0x55)
#define ICC_RISPA_PAYLOAD_SIZE (ICC_NETWORK_1D_CELL_COUNT * 4U)
#define ICC_RISPA_FRAME_SIZE (2U + ICC_RISPA_PAYLOAD_SIZE)

bool icc_rispa_encode_voltage_frame(
    const int32_t voltage_nv[ICC_NETWORK_1D_CELL_COUNT],
    uint8_t frame[ICC_RISPA_FRAME_SIZE]);

#endif
