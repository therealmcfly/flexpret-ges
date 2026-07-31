#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "icc.h"

static void test_initial_wait_and_first_upstroke(void)
{
    Icc cell;
    icc_init(&cell, 20);

    for (int i = 0; i < 24; ++i) {
        (void)icc_step(&cell);
        assert(cell.state == ICC_WAIT);
    }

    (void)icc_step(&cell);
    assert(cell.state == ICC_Q0_RESTING);
    assert(cell.voltage_nv == -67647596);

    (void)icc_step(&cell);
    assert(cell.state == ICC_Q1_UPSTROKE);
    assert(cell.voltage_nv == -58942636);
}

static void test_follower_requires_relay(void)
{
    Icc cell;
    icc_init(&cell, 0);

    for (int i = 0; i < 40; ++i) {
        (void)icc_step(&cell);
    }
    assert(cell.state == ICC_Q0_RESTING);

    cell.relay = 1;
    (void)icc_step(&cell);
    assert(cell.state == ICC_Q1_UPSTROKE);
}

static void test_blocked_cell_absorbs_relay(void)
{
    Icc cell;
    icc_init(&cell, -1);

    for (int i = 0; i < 25; ++i) {
        (void)icc_step(&cell);
    }

    cell.relay = 1;
    (void)icc_step(&cell);
    assert(cell.state == ICC_Q0_RESTING);
    assert(cell.relay == 0);
}

static void test_nanovolt_accumulation(void)
{
    Icc cell;
    icc_init(&cell, 20);

    for (int i = 0; i < 81; ++i) {
        (void)icc_step(&cell);
    }

    assert(cell.state == ICC_Q0_RESTING);
    assert(cell.voltage_nv == -67013996);

    (void)icc_step(&cell);
    assert(cell.voltage_nv == -67027992);
}

static void test_display_rounding(void)
{
    Icc cell;
    icc_init(&cell, 20);

    cell.voltage_nv = -4494;
    assert(icc_voltage_nearest_uv(&cell) == -4);

    cell.voltage_nv = -4500;
    assert(icc_voltage_nearest_uv(&cell) == -4);

    cell.voltage_nv = -4501;
    assert(icc_voltage_nearest_uv(&cell) == -5);

    cell.voltage_nv = 4494;
    assert(icc_voltage_nearest_uv(&cell) == 4);

    cell.voltage_nv = 4500;
    assert(icc_voltage_nearest_uv(&cell) == 5);
}

int main(void)
{
    assert(sizeof(IccVoltageNv) == sizeof(int32_t));
    test_initial_wait_and_first_upstroke();
    test_follower_requires_relay();
    test_blocked_cell_absorbs_relay();
    test_nanovolt_accumulation();
    test_display_rounding();
    puts("icc-model tests passed");
    return 0;
}
