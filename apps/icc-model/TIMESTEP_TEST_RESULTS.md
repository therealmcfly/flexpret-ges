# Verilator Timestep Sensitivity Results

## Purpose

This campaign tests the five-cell ICC model at biological timesteps of 200,
100, 50, 20, and 10 ms. The maximum permitted timestep is 200 ms. Every tested
timestep divides the calibrated 200 ms base step exactly.

The production configuration remains 200 ms. Smaller timesteps are currently
experimental because their per-step nanovolt increments are obtained by signed
integer scaling and round-to-nearest from the calibrated 200 ms increments.

## Integer scaling method

For a calibrated 200 ms voltage increment `increment_200`, the experimental
increment is computed at compile time as:

```text
increment_dt = round(increment_200 * timestep_ms / 200)
```

The target executes only integer additions. No floating-point operation is
introduced. However, rounding every small step independently can accumulate a
different total voltage change and can move a threshold crossing to a different
sample. Timestep sensitivity must therefore be measured rather than assumed.

## Intrinsic frequency accuracy

The following table gives the measured Q1-to-Q1 period in milliseconds. The
configured periods correspond to 4.000, 3.000, 2.609, 2.308, 2.000, and 1.500
cpm respectively.

| Timestep | 15 s target | 20 s target | 23 s target | 26 s target | 30 s target | 40 s target |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 200 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40000 |
| 100 ms | 14900 | 19800 | 22900 | 25900 | 29800 | 39800 |
| 50 ms | 14850 | 19750 | 22850 | 25800 | 29700 | 39700 |
| 20 ms | 14780 | 19660 | 22780 | 25740 | 29640 | 39620 |
| 10 ms | 14790 | 19740 | 22870 | 25890 | 29840 | 39890 |

| Timestep | Maximum absolute period error | Maximum relative error |
|---:|---:|---:|
| 200 ms | 0 ms | 0% |
| 100 ms | 200 ms | 1.00% |
| 50 ms | 300 ms | 1.25% |
| 20 ms | 380 ms | 1.70% |
| 10 ms | 260 ms | 1.40% |

The error is not monotonic with timestep because voltage increments and state
threshold crossings are both quantized. A smaller timestep does not
automatically provide a more accurate configured cpm when each step uses one
rounded integer increment.

The 10 ms values were additionally confirmed directly in Verilator using five
all-equal networks. Because every cell had the same intrinsic period, all paths
annihilated simultaneous wavefronts and no path-induced Q1 event occurred:

| Configured period | Verilator Q1-to-Q1 period | Error | Measured frequency |
|---:|---:|---:|---:|
| 20 s | 19740 ms | -260 ms | 3.039514 cpm |
| 23 s | 22870 ms | -130 ms | 2.623524 cpm |
| 26 s | 25890 ms | -110 ms | 2.317497 cpm |
| 30 s | 29840 ms | -160 ms | 2.010724 cpm |
| 40 s | 39890 ms | -110 ms | 1.504136 cpm |

Across these five runs, all 120 recorded Q1 events were intrinsic and none were
path induced. The discrepancy is therefore produced by timestep quantization,
not network entrainment.

An offline search was also performed for a constant integer resting increment
that would restore the exact configured period. Exact constants were available
for many timestep/frequency combinations, but not for all. In particular, no
constant nanovolt-per-step value produced the exact period for the 40-second
cell at 20 ms, or for the 23-, 26-, 30-, and 40-second cells at 10 ms under the
current state equations. A lookup table alone therefore cannot make every
tested timestep exact.

## Network scenario matrix

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
models reported four. This is consistent with their slightly shortened rounded
intrinsic periods and demonstrates that numerical timestep error can affect
which cell wins a close propagation-versus-intrinsic timing race.

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
This demonstrates computational feasibility for this network size, but it does
not correct the cpm drift caused by rounded voltage increments.

## Conclusion

The 200 ms configuration remains the only tested mode that simultaneously
provides:

- exact configured intrinsic periods;
- exact integer path delays;
- the validated ICC waveform constants; and
- hard real-time scheduling with 160 ns maximum release lateness in Verilator.

Smaller timesteps are computationally feasible and retain exact path timing,
but the simple constant-increment representation introduces up to 1.70% cpm
error and can change close pacemaker-competition boundaries. If exact cpm is
required below 200 ms, the next design decision is between a fractional
remainder accumulator and a different time-based phase representation. The
smaller-timestep modes should remain experimental until that choice is made.
