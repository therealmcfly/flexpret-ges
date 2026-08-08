# Verilator Timestep Sensitivity Results

## Purpose

This campaign tests the five-cell ICC model at biological timesteps of 200,
100, 50, 20, and 10 ms. The maximum permitted timestep is 200 ms. Every tested
timestep divides the calibrated 200 ms base step exactly.

The production configuration remains 200 ms. Smaller timesteps remain
experimental, but their Q0 resting increments are now selected from an
exhaustively searched calibration table rather than obtained by directly
scaling the 200 ms resting increments.

## Integer calibration method

For each supported timestep and pacemaker interval, an offline exhaustive
search executes the real ICC state machine with candidate constant integer Q0
increments. It selects the value with the minimum absolute Q1-to-Q1 period
error. When candidates tie, a period that does not activate early is preferred,
followed by the value closest to the mathematically scaled 200 ms increment.

The selected values are compiled into `inc/icc_calibration.h`. Runtime
execution remains a single integer addition and introduces no floating-point
operation or additional cell state.

## Intrinsic frequency accuracy

The following table gives the measured Q1-to-Q1 period in milliseconds:

| Timestep | 15 s target | 20 s target | 23 s target | 26 s target | 30 s target | 40 s target |
|---:|---:|---:|---:|---:|---:|---:|
| 200 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40000 |
| 100 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40000 |
| 50 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40000 |
| 20 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40020 |
| 10 ms | 15000 | 20000 | 23010 | 25990 | 30010 | 40020 |

| Timestep | Maximum absolute period error | Maximum relative error |
|---:|---:|---:|
| 200 ms | 0 ms | 0% |
| 100 ms | 0 ms | 0% |
| 50 ms | 0 ms | 0% |
| 20 ms | 20 ms | 0.05% |
| 10 ms | 20 ms | 0.05% |

The complete host ICC/path/network suite passed at all five timesteps. A
25-case all-equal Verilator matrix additionally confirmed the 20-, 23-, 26-,
30-, and 40-second measurements at every timestep. Every Q1 event in those
all-equal cases was intrinsic; simultaneous wavefronts were annihilated.

At 10 ms, the directly confirmed Verilator results were:

| Configured period | Verilator Q1-to-Q1 period | Error | Measured frequency |
|---:|---:|---:|---:|
| 20 s | 20000 ms | 0 ms | 3.000000 cpm |
| 23 s | 23010 ms | +10 ms | 2.607562 cpm |
| 26 s | 25990 ms | -10 ms | 2.308580 cpm |
| 30 s | 30010 ms | +10 ms | 1.999334 cpm |
| 40 s | 40020 ms | +20 ms | 1.499250 cpm |

A single constant integer increment cannot produce every exact period under the
current threshold equations. No exact value exists for the 40-second cell at
20 ms, or for the 23-, 26-, 30-, and 40-second cells at 10 ms. The selected
lookup values reduce the worst observed error from 380 ms to 20 ms.

## Network scenario matrix

> Note: the 100-case dominance matrix below was recorded before the new
> timestep-specific Q0 calibration table. Its principal path conclusions remain
> useful, but close 4- and 5-second competition boundaries have not yet been
> rerun with the new lookup values.

One hundred Verilator cases were run. Each case represented 120 seconds of
biological time. The matrix contained:

- fastest 3 cpm cell at the left;
- fastest 3 cpm cell at the centre;
- fastest 3 cpm cell at the right;
- equal 3 cpm cells at both ends;
- timesteps of 200, 100, 50, 20, and 10 ms; and
- uniform path delays of 1, 2, 3, 4, and 5 seconds.

For functional testing, the waiting period inside Verilator was accelerated to
`10000 ns`, but `icc_step()` and `icc_path_step()` still represented the
selected biological timestep. True-period deadline tests were run separately.

All 100 cases reached their expected 120-second completion marker.

## Pacemaker dominance

The main dominance boundary was identical at every timestep:

| Configuration | 1 s paths | 2 s paths | 3 s paths | 4 s paths | 5 s paths |
|---|---|---|---|---|---|
| Fastest left | Dominant | Dominant | Dominant | Competing | Competing |
| Fastest centre | Dominant | Dominant | Dominant | Competing | Competing |
| Fastest right | Dominant | Dominant | Dominant | Competing | Competing |
| Equal fastest ends | Co-dominant | Co-dominant | Co-dominant | Additional intrinsic sites | Additional intrinsic sites |

Thus, reducing the timestep did not change the principal conclusion: the 3 cpm
cell entrained the network when each path delay was at most 3 seconds. At 4 and
5 seconds, slower cells could reach intrinsic Q1 before the propagated wave
arrived.

Some boundary details changed. With the fastest cell at an end and a 4-second
delay, the 200 ms model reported three intrinsic sites, while the smaller-step
models reported four. These results came from the earlier direct-scaling
implementation and have not been rerun with the calibrated lookup.

## Path-delay accuracy

Across the unique-fastest scenarios, 1416 path-induced activations had an
unambiguous preceding activation in the neighboring cell. Every checked event
matched the configured delay exactly:

```text
exact delay matches = 1416 / 1416
```

Path timing remains exact because all path delays are integer multiples of each
tested biological timestep.

## True-period FlexPRET timing

Separate Verilator runs used the actual scheduler period corresponding to each
biological timestep. The test recorded the minimum and maximum measured period
and maximum release lateness.

| Timestep | Requested period | Minimum measured | Maximum measured | Maximum release lateness |
|---:|---:|---:|---:|---:|
| 200 ms | 200000000 ns | 200000000 ns | 200000160 ns | 160 ns |
| 100 ms | 100000000 ns | 100000000 ns | 100000160 ns | 160 ns |
| 50 ms | 50000000 ns | 50000000 ns | 50000160 ns | 160 ns |
| 20 ms | 20000000 ns | 20000000 ns | 20000160 ns | 160 ns |
| 10 ms | 10000000 ns | 10000000 ns | 10000160 ns | 160 ns |

The five-cell workload met every tested deadline down to 10 ms in Verilator.
This demonstrates computational feasibility for this network size. Scheduler
timing is independent of the separately calibrated Q0 period accuracy.

## DE1-SoC hardware validation

The calibrated five-cell network was subsequently tested on the physical
DE1-SoC board on 8-9 August 2026. FlexPRET and its UART bootloader were already
programmed into the FPGA. Each diagnostic application was built with
`TARGET=fpga`, flashed through `/dev/ttyUSB0`, and run after resetting the board
with SW0 high and KEY3 pressed.

The production network configuration was used for every run: cell 4 was the
20-second intrinsic pacemaker, cells 0-3 were followers, and each of the four
paths had a one-second delay. The following measurements therefore validate
the calibrated 20-second cell and path propagation on hardware; the other
calibrated intrinsic intervals are covered by the host and Verilator matrices
above.

| Timestep | Measured Q1-to-Q1 interval | Cell-to-cell propagation | Measured loop period | Release lateness |
|---:|---:|---:|---:|---:|
| 200 ms | 20000 ms | 1000 ms | 200000000 ns | 160 ns |
| 100 ms | 20000 ms | 1000 ms | 100000000 ns | 160 ns |
| 50 ms | 20000 ms | 1000 ms | 50000000 ns | 160 ns |
| 20 ms | 20000 ms | 1000 ms | 20000000 ns | 160 ns |
| 10 ms | 20000 ms | 1000 ms | 10000000 ns | 160 ns |

For example, the clean 10 ms event-only capture recorded cell 4 entering Q1
at model times 85010 ms and 105010 ms. Cells 3, 2, 1, and 0 then entered Q1 at
86010, 87010, 88010, and 89010 ms respectively. This gives an exact 20-second
pacemaker interval and four exact one-second propagation steps.

Full per-iteration CSV telemetry remained usable through the 20 ms test, but
the receiver skipped some rows at that rate because of UART bandwidth. At
10 ms, full CSV output saturated the serial connection and was not considered
a valid timing test. The 10 ms run was rebuilt with temporary event-only
telemetry that emitted a row only when a cell entered Q1. This removed the UART
load while retaining the FPGA `rdtime()` period and release-lateness fields.
The temporary diagnostic guards and event filter were removed after testing
and are not part of the production application.

These board measurements agree with the true-period Verilator results and
confirm that the five-cell workload meets its deadlines on the deployed
FlexPRET hardware for every supported timestep.

## Conclusion

The 200 ms configuration remains the production mode and simultaneously
provides:

- exact configured intrinsic periods;
- exact integer path delays;
- the validated ICC waveform constants; and
- hard real-time scheduling with 160 ns maximum release lateness in Verilator.

Smaller timesteps are computationally feasible and retain exact path timing,
but the timestep-specific calibration limits the measured intrinsic-period
error to 20 ms. If exact cpm is required below 200 ms, the next
design decision is between a fractional remainder accumulator and a different time-based phase representation. The
smaller-timestep modes should remain experimental until that choice is made.
