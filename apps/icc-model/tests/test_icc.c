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
    assert(cell.voltage_nv == -67647907);

    (void)icc_step(&cell);
    assert(cell.state == ICC_Q1_UPSTROKE);
    assert(cell.voltage_nv == -58942947);
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
    assert(cell.voltage_nv == -67014307);

    (void)icc_step(&cell);
    assert(cell.voltage_nv == -67028614);
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

static int next_q1_sample(Icc *cell, int after_sample)
{
    IccState previous = cell->state;

    for (int sample = after_sample + 1; sample < 2000; ++sample) {
        (void)icc_step(cell);
        if (cell->state == ICC_Q1_UPSTROKE && previous != ICC_Q1_UPSTROKE) {
            return sample;
        }
        previous = cell->state;
    }

    return -1;
}

static void test_exact_pacemaker_intervals(void)
{
    static const int intervals_s[] = {15, 20, 23, 26, 30, 40};

    for (size_t i = 0; i < sizeof(intervals_s) / sizeof(intervals_s[0]); ++i) {
        Icc cell;
        icc_init(&cell, (int8_t)intervals_s[i]);

        int first_q1 = next_q1_sample(&cell, 0);
        int second_q1 = next_q1_sample(&cell, first_q1);
        int elapsed_ms = (second_q1 - first_q1) * (int)ICC_TIMESTEP_MS;

        assert(elapsed_ms == intervals_s[i] * 1000);
    }
}

int main(void)
{
    assert(sizeof(IccVoltageNv) == sizeof(int32_t));
    test_initial_wait_and_first_upstroke();
    test_follower_requires_relay();
    test_blocked_cell_absorbs_relay();
    test_nanovolt_accumulation();
    test_display_rounding();
    test_exact_pacemaker_intervals();
    puts("icc-model tests passed");
    return 0;
}
