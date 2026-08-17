#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "icc.h"
#include "icc_calibration.h"
#include "network.h"
#include "path.h"

static int32_t scale_200ms_increment(int32_t value)
{
    return (int32_t)((((int64_t)value * (int64_t)ICC_TIMESTEP_MS) +
        (value >= 0 ? 100 : -100)) / 200);
}

static void test_initial_wait_and_first_upstroke(void)
{
    Icc cell;
    const uint32_t wait_steps =
        (ICC_WAIT_MS + ICC_TIMESTEP_MS - 1U) / ICC_TIMESTEP_MS;
    icc_init(&cell, 20);

    for (uint32_t i = 0U; i < wait_steps - 1U; ++i) {
        (void)icc_step(&cell);
        assert(cell.state == ICC_WAIT);
    }

    (void)icc_step(&cell);
    assert(cell.state == ICC_Q0_RESTING);
    assert(cell.voltage_nv ==
           -67633600 + ICC_RESTING_20S_NV);

    (void)icc_step(&cell);
    assert(cell.state == ICC_Q1_UPSTROKE);
    assert(cell.voltage_nv ==
           -67633600 + ICC_RESTING_20S_NV +
           scale_200ms_increment(8704960));
}

static void test_follower_requires_relay(void)
{
    Icc cell;
    icc_init(&cell, 0);

    for (uint32_t i = 0U; i < 8000U / ICC_TIMESTEP_MS; ++i) {
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

    for (uint32_t i = 0U;
         i < (ICC_WAIT_MS + ICC_TIMESTEP_MS - 1U) / ICC_TIMESTEP_MS;
         ++i) {
        (void)icc_step(&cell);
    }

    cell.relay = 1;
    (void)icc_step(&cell);
    assert(cell.state == ICC_Q0_RESTING);
    assert(cell.relay == 0);
}

#if ICC_TIMESTEP_MS == 200U
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
#endif

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

    for (int sample = after_sample + 1;
         sample < 100000 / (int)ICC_TIMESTEP_MS;
         ++sample) {
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
#if ICC_TIMESTEP_MS == 20U
    static const int expected_periods_ms[] = {
        15000, 20000, 23000, 26000, 30000, 40020
    };
#elif ICC_TIMESTEP_MS == 10U
    static const int expected_periods_ms[] = {
        15000, 20000, 23010, 25990, 30010, 40020
    };
#else
    static const int expected_periods_ms[] = {
        15000, 20000, 23000, 26000, 30000, 40000
    };
#endif

    for (size_t i = 0; i < sizeof(intervals_s) / sizeof(intervals_s[0]); ++i) {
        Icc cell;
        icc_init(&cell, (int8_t)intervals_s[i]);

        int first_q1 = next_q1_sample(&cell, 0);
        int second_q1 = next_q1_sample(&cell, first_q1);
        int elapsed_ms = (second_q1 - first_q1) * (int)ICC_TIMESTEP_MS;

        assert(elapsed_ms == expected_periods_ms[i]);
    }
}

static void set_resting(Icc *cell)
{
    cell->state = ICC_Q0_RESTING;
    cell->voltage_nv = -67633600;
    cell->wait_ms_accum = 0U;
    cell->relay = 0;
}

static void test_path_relays_a_to_b_at_effective_delay(void)
{
    Icc cell_a;
    Icc cell_b;
    IccPath path;

    icc_init(&cell_a, 0);
    icc_init(&cell_b, 0);
    set_resting(&cell_a);
    set_resting(&cell_b);
    cell_a.state = ICC_Q1_UPSTROKE;

    icc_path_init(&path, &cell_a, &cell_b, 1000U, 6U);
    icc_path_step(&path);
    assert(path.state == ICC_PATH_CELL_A_WAIT);
    assert(path.active_time_ms[0] == 0);
    assert(path.active_time_ms[1] == ICC_PATH_INACTIVE_TIME_MS);

    for (uint32_t step = 0U;
         step < 1000U / ICC_TIMESTEP_MS - 1U;
         ++step) {
        icc_path_step(&path);
    }

    assert(path.state == ICC_PATH_CELL_A_RELAY);
    assert(path.elapsed_ms == 1000U - ICC_TIMESTEP_MS);
    assert(cell_b.relay == 1);

    (void)icc_step(&cell_b);
    assert(cell_b.state == ICC_Q1_UPSTROKE);
    assert(cell_b.relay == 0);

    icc_path_step(&path);
    assert(path.state == ICC_PATH_IDLE);
    assert(path.active_time_ms[0] == ICC_PATH_INACTIVE_TIME_MS);
}

static void test_path_relays_b_to_a(void)
{
    Icc cell_a;
    Icc cell_b;
    IccPath path;

    icc_init(&cell_a, 0);
    icc_init(&cell_b, 0);
    set_resting(&cell_a);
    set_resting(&cell_b);
    cell_b.state = ICC_Q1_UPSTROKE;

    icc_path_init(&path, &cell_a, &cell_b, 1000U, 6U);
    icc_path_step(&path);
    assert(path.state == ICC_PATH_CELL_B_WAIT);

    for (uint32_t step = 0U;
         step < 1000U / ICC_TIMESTEP_MS - 1U;
         ++step) {
        icc_path_step(&path);
    }

    assert(path.state == ICC_PATH_CELL_B_RELAY);
    assert(path.active_time_ms[1] ==
           (int32_t)(1000U - ICC_TIMESTEP_MS));
    assert(cell_a.relay == 1);
}

static void test_path_annihilates_simultaneous_activation(void)
{
    Icc cell_a;
    Icc cell_b;
    IccPath path;

    icc_init(&cell_a, 0);
    icc_init(&cell_b, 0);
    cell_a.state = ICC_Q1_UPSTROKE;
    cell_b.state = ICC_Q1_UPSTROKE;
    icc_path_init(&path, &cell_a, &cell_b, 1000U, 6U);

    icc_path_step(&path);
    assert(path.state == ICC_PATH_ANNIHILATE);
    assert(cell_a.relay == 0);
    assert(cell_b.relay == 0);

    cell_a.state = ICC_Q2_PLATEAU;
    cell_b.state = ICC_Q2_PLATEAU;
    icc_path_step(&path);
    assert(path.state == ICC_PATH_ANNIHILATE);

    cell_a.state = ICC_Q0_RESTING;
    cell_b.state = ICC_Q0_RESTING;
    icc_path_step(&path);
    assert(path.state == ICC_PATH_IDLE);
}

static void test_active_states_match_iccnet_core(void)
{
    Icc cell;

    icc_init(&cell, 0);
    cell.state = ICC_Q0_RESTING;
    assert(!icc_is_active(&cell));
    cell.state = ICC_Q1_UPSTROKE;
    assert(icc_is_active(&cell));
    cell.state = ICC_Q2_PLATEAU;
    assert(icc_is_active(&cell));
    cell.state = ICC_Q3_REPOLARIZATION;
    assert(icc_is_active(&cell));
    cell.state = ICC_WAIT;
    assert(!icc_is_active(&cell));
}

static void test_relay_handoff_does_not_reflect(void)
{
    Icc cell_a;
    Icc cell_b;
    IccPath path;

    icc_init(&cell_a, 0);
    icc_init(&cell_b, 0);
    cell_a.state = ICC_Q2_PLATEAU;
    cell_b.state = ICC_Q1_UPSTROKE;
    icc_path_init(&path, &cell_a, &cell_b, 1000U, 6U);

    icc_path_step(&path);
    assert(path.state == ICC_PATH_ANNIHILATE);
    assert(path.active_time_ms[0] == ICC_PATH_INACTIVE_TIME_MS);
    assert(path.active_time_ms[1] == ICC_PATH_INACTIVE_TIME_MS);
    assert(cell_a.relay == 0);
    assert(cell_b.relay == 0);
}

static void test_blocked_destination_absorbs_path_relay(void)
{
    Icc cell_a;
    Icc cell_b;
    IccPath path;

    icc_init(&cell_a, 0);
    icc_init(&cell_b, -1);
    set_resting(&cell_a);
    set_resting(&cell_b);
    cell_a.state = ICC_Q1_UPSTROKE;
    icc_path_init(&path, &cell_a, &cell_b, 1000U, 6U);

    icc_path_step(&path);
    for (uint32_t step = 0U;
         step < 1000U / ICC_TIMESTEP_MS - 1U;
         ++step) {
        icc_path_step(&path);
    }
    assert(cell_b.relay == 1);

    (void)icc_step(&cell_b);
    assert(cell_b.state == ICC_Q0_RESTING);
    assert(cell_b.relay == 0);
}

static void test_five_cell_1d_network_propagation(void)
{
    const int32_t pacemaker_first_q1_ms =
        (int32_t)(((ICC_WAIT_MS + ICC_TIMESTEP_MS - 1U) /
                   ICC_TIMESTEP_MS + 1U) * ICC_TIMESTEP_MS);
    int32_t expected_first_q1_ms[ICC_NETWORK_1D_CELL_COUNT];
    int32_t first_q1_ms[ICC_NETWORK_1D_CELL_COUNT] = {-1, -1, -1, -1, -1};
    IccNetwork1d network;
    const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {0, 0, 0, 0, 20};
    const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        1000U, 1000U, 1000U, 1000U
    };
    const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {6U, 6U, 6U, 6U};

    assert(icc_network_1d_init(&network, intervals, delays, gaps));

    for (uint8_t index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        expected_first_q1_ms[index] =
            pacemaker_first_q1_ms +
            (int32_t)(ICC_NETWORK_1D_CELL_COUNT - 1U - index) * 1000;
    }

    for (uint8_t index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        assert(network.paths[index].cell_a == &network.cells[index]);
        assert(network.paths[index].cell_b == &network.cells[index + 1U]);
        assert(network.paths[index].delay_ms == 1000U);
        assert(network.paths[index].gap_mm == 6U);
    }

    for (int32_t sample = 1;
         sample <= 20000 / (int32_t)ICC_TIMESTEP_MS;
         ++sample) {
        icc_network_1d_step(&network);

        for (uint8_t index = 0U;
             index < ICC_NETWORK_1D_CELL_COUNT;
             ++index) {
            if (first_q1_ms[index] < 0 &&
                network.cells[index].state == ICC_Q1_UPSTROKE) {
                first_q1_ms[index] = sample * (int32_t)ICC_TIMESTEP_MS;
            }
        }
    }

    for (uint8_t index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        assert(first_q1_ms[index] == expected_first_q1_ms[index]);
    }
}

static void test_five_cell_network_configuration(void)
{
    const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
        -1, 15, 23, 30, 40
    };
    const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        400U, 600U, 800U, 1000U
    };
    const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {3U, 4U, 5U, 6U};
    IccNetwork1d network;

    assert(icc_network_1d_init(&network, intervals, delays, gaps));
    for (uint8_t index = 0U; index < ICC_NETWORK_1D_CELL_COUNT; ++index) {
        assert(network.cells[index].pacemaker_interval_s == intervals[index]);
    }
    for (uint8_t index = 0U; index < ICC_NETWORK_1D_PATH_COUNT; ++index) {
        assert(network.paths[index].delay_ms == delays[index]);
        assert(network.paths[index].gap_mm == gaps[index]);
    }
}

static void test_five_cell_network_rejects_invalid_configuration(void)
{
    int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {0, 0, 0, 0, 20};
    uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
        1000U, 1000U, 1000U, 1000U
    };
    uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {6U, 6U, 6U, 6U};
    IccNetwork1d network;

    intervals[2] = 17;
    assert(!icc_network_1d_init(&network, intervals, delays, gaps));
    intervals[2] = 0;

    delays[1] = ICC_TIMESTEP_MS;
    assert(!icc_network_1d_init(&network, intervals, delays, gaps));
    delays[1] = 1000U;

    gaps[3] = 0U;
    assert(!icc_network_1d_init(&network, intervals, delays, gaps));
}

int main(void)
{
    assert(sizeof(IccVoltageNv) == sizeof(int32_t));
    test_initial_wait_and_first_upstroke();
    test_follower_requires_relay();
    test_blocked_cell_absorbs_relay();
#if ICC_TIMESTEP_MS == 200U
    test_nanovolt_accumulation();
#endif
    test_display_rounding();
    test_exact_pacemaker_intervals();
    test_path_relays_a_to_b_at_effective_delay();
    test_path_relays_b_to_a();
    test_active_states_match_iccnet_core();
    test_path_annihilates_simultaneous_activation();
    test_relay_handoff_does_not_reflect();
    test_blocked_destination_absorbs_path_relay();
    test_five_cell_1d_network_propagation();
    test_five_cell_network_configuration();
    test_five_cell_network_rejects_invalid_configuration();
    puts("icc-model tests passed");
    return 0;
}
