#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "rispa_frame.h"

static void test_voltage_frame(void)
{
    static const int32_t voltage_nv[ICC_NETWORK_1D_CELL_COUNT] = {
        0, 1, -1, INT32_MIN, INT32_MAX
    };
    uint8_t frame[ICC_RISPA_FRAME_SIZE];

    memset(frame, 0, sizeof(frame));
    assert(icc_rispa_encode_voltage_frame(voltage_nv, frame));
    assert(ICC_RISPA_FRAME_SIZE == 22U);
    assert(frame[0] == UINT8_C(0xAA));
    assert(frame[1] == UINT8_C(0x55));
    assert(frame[2] == 0U && frame[3] == 0U &&
        frame[4] == 0U && frame[5] == 0U);
    assert(frame[6] == 1U && frame[7] == 0U &&
        frame[8] == 0U && frame[9] == 0U);
    assert(frame[10] == UINT8_C(0xFF) && frame[11] == UINT8_C(0xFF) &&
        frame[12] == UINT8_C(0xFF) && frame[13] == UINT8_C(0xFF));
    assert(frame[14] == 0U && frame[15] == 0U &&
        frame[16] == 0U && frame[17] == UINT8_C(0x80));
    assert(frame[18] == UINT8_C(0xFF) && frame[19] == UINT8_C(0xFF) &&
        frame[20] == UINT8_C(0xFF) && frame[21] == UINT8_C(0x7F));

    assert(!icc_rispa_encode_voltage_frame(NULL, frame));
    assert(!icc_rispa_encode_voltage_frame(voltage_nv, NULL));
}

int main(void)
{
    test_voltage_frame();
    printf("RiSPA voltage frame tests passed\n");
    return 0;
}
