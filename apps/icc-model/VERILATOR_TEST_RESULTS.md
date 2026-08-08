# Verilator Pacemaker and Path-Delay Tests

## Scope

This test campaign evaluates the five-cell one-dimensional ICC network with the
fastest intrinsic frequency placed at different locations, two equal fastest
cells, and uniform path delays from 1 to 5 seconds. Frequencies are reported in
cycles per minute (cpm), while the implementation stores integer cycle periods
in seconds.

Each case executes 1000 model steps, representing 200 seconds at the model's
200 ms timestep. Events before 20 seconds are treated as startup transients and
are excluded from the steady-state classification.

## Verilator method

A benchmark using the real `200000000 ns` scheduler period produced six model
steps in approximately 12 seconds of wall time. Running all 40 cases at that
cycle-accurate waiting period would therefore require more than 20 hours.

The functional matrix uses an emulator-only compile-time test mode. The ICC and
path state machines still advance once per 200 ms biological model step, but
the FlexPRET waiting period between calls is shortened to `100000 ns`. Test mode
also stops after 1000 steps and prints only Q1 and annihilation events. These
changes are excluded from the ordinary emulator and FPGA builds.

The accelerated results validate discrete ICC/path behavior, propagation, and
pacemaker competition. They do not constitute a hard real-time timing
measurement. A separate true-period Verilator run measured a steady
`200000000 ns` period with `160 ns` release lateness.

One case can be reproduced as follows. Scenario `0` selects the left-fastest
configuration and the command below selects a 1000 ms uniform path delay:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cd apps/icc-model

cmake -S . -B build-verilator-tests \
  -DTARGET=emulator \
  -DICC_VERILATOR_TEST_SCENARIO=0 \
  -DICC_VERILATOR_TEST_PATH_DELAY_MS=1000 \
  -DICC_VERILATOR_TEST_SAMPLES=1000 \
  -DICC_VERILATOR_TEST_PERIOD_NS=100000
cmake --build build-verilator-tests --target icc-model
fp-emu +ispm=build-verilator-tests/icc-model.mem
```

Setting `ICC_VERILATOR_TEST_SCENARIO` to an empty value restores the ordinary
emulator program and its real 200 ms scheduler.

## Tested cell configurations

| Scenario | Periods in code (s) | Frequencies (cpm) |
|---:|---|---|
| 0: fastest left | `{20,23,26,30,40}` | `{3.000,2.609,2.308,2.000,1.500}` |
| 1: fastest Cell 1 | `{23,20,26,30,40}` | `{2.609,3.000,2.308,2.000,1.500}` |
| 2: fastest centre | `{30,23,20,26,40}` | `{2.000,2.609,3.000,2.308,1.500}` |
| 3: fastest Cell 3 | `{40,30,26,20,23}` | `{1.500,2.000,2.308,3.000,2.609}` |
| 4: fastest right | `{40,30,26,23,20}` | `{1.500,2.000,2.308,2.609,3.000}` |
| 5: equal fastest ends | `{20,23,26,23,20}` | `{3.000,2.609,2.308,2.609,3.000}` |
| 6: equal fastest inner | `{30,20,26,20,30}` | `{2.000,3.000,2.308,3.000,2.000}` |
| 7: all equal | `{20,20,20,20,20}` | `{3.000,3.000,3.000,3.000,3.000}` |

Every scenario was tested with all four paths set uniformly to 1000, 2000,
3000, 4000, and 5000 ms. Path gaps remained fixed at 6 mm so that delay was the
only path variable.

## Main results

| Configuration | 1 s paths | 2 s paths | 3 s paths | 4 s paths | 5 s paths |
|---|---|---|---|---|---|
| Unique fastest: left | Dominant | Dominant | Dominant | Competing | Competing |
| Unique fastest: Cell 1 | Dominant | Dominant | Dominant | Competing | Competing |
| Unique fastest: centre | Dominant | Dominant | Dominant | Competing | Competing |
| Unique fastest: Cell 3 | Dominant | Dominant | Dominant | Competing | Competing |
| Unique fastest: right | Dominant | Dominant | Dominant | Competing | Competing |
| Equal fastest: ends | Co-dominant | Co-dominant | Co-dominant | Additional intrinsic sites | Additional intrinsic sites |
| Equal fastest: inner | Co-dominant | Co-dominant | Co-dominant | Co-dominant | Co-dominant |
| All cells at 3 cpm | Synchronous | Synchronous | Synchronous | Synchronous | Synchronous |

For all five unique-fastest arrangements, delays of 1 to 3 seconds allowed the
3 cpm cell to become the sole steady-state intrinsic source. Every other cell
was activated through a path and exhibited a 20-second observed period, equal
to 3 cpm.

At delays of 4 and 5 seconds, slower cells sometimes reached Q1 intrinsically
before the fastest cell's propagated wave arrived. The competing intrinsic
sites were:

| Fastest position | 4 s paths | 5 s paths |
|---|---|---|
| Cell 0 | Cells 0, 1, 2 | Cells 0, 1, 2, 3 |
| Cell 1 | Cells 0, 1 | Cells 0, 1 |
| Cell 2 | Cells 1, 2 | Cells 1, 2 |
| Cell 3 | Cells 3, 4 | Cells 3, 4 |
| Cell 4 | Cells 2, 3, 4 | Cells 1, 2, 3, 4 |

This does not indicate a numerical failure. It demonstrates that the highest
intrinsic frequency becomes globally dominant only when conduction reaches the
other cells before their intrinsic activation.

## Propagation accuracy

Across the unique-fastest scenarios, 876 path-induced activations had an
unambiguous preceding neighboring-cell activation. All 876 occurred exactly
one configured path delay later:

```text
exact delay matches = 876 / 876
```

The result covers uniform delays of 1000, 2000, 3000, 4000, and 5000 ms and
propagation in both directions.

## Equal-frequency behavior

With equal 3 cpm pacemakers at both ends, both end cells remained intrinsic
sources for delays of 1 to 3 seconds. At 4 and 5 seconds, every cell produced at
least one steady-state intrinsic activation in addition to path activations.

With equal 3 cpm pacemakers at Cells 1 and 3, those two cells remained the only
intrinsic sources at every tested delay. Their maximum distance to another cell
is one path, so even a 5-second conduction delay reached the neighboring cells
before those slower cells completed their intrinsic cycles.

When all cells were configured at 3 cpm, all five activated intrinsically and
synchronously. No path-induced Q1 events occurred. Each of the four paths
entered `ANNIHILATE` once per steady-state cycle, producing 36 annihilation
events over the nine classified cycles in each case.

In the two-ended and two-inner pacemaker cases, converging waves reached the
same cell rather than entering opposite ends of one path simultaneously.
Consequently, the central cell activated once, but the path state machine did
not report a steady-state `ANNIHILATE` event.

## Startup transient

All five cells entered their first Q1 state simultaneously at 5200 ms in every
configuration, regardless of intrinsic frequency. All four paths consequently
entered `ANNIHILATE` at that instant. The configured periods affected later
cycles, after this shared initial phase.

This behavior follows from initializing every cell in the same startup state.
It must be reported as an initialization transient and should not be interpreted
as pacemaker dominance. Steady-state analysis therefore begins at 20 seconds.

## Completion and production verification

All 40 cases reached the expected completion marker:

```text
DONE,1000,200000
```

After the matrix, the emulator-only test mode was disabled and the following
checks passed:

- host ICC/path/network tests;
- ordinary Verilator build; and
- ordinary FPGA build.

The production FPGA target therefore retains its 200 ms scheduling period and
autonomous compiled configuration.
