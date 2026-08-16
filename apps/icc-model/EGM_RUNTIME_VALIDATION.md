# Relative-Potential EGM Runtime Validation

Validation date: 2026-08-16

## Host runtime matrix

Command:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/generate_egm_luts.sh
./tools/run_egm_runtime_tests.sh
```

The test compiled the production `src/egm.c`, ICC, path, and network code
separately for 200, 100, 50, 20, and 10 ms. All five executables passed.

For each timestep, automated assertions covered:

- all five electrode coordinates;
- all four path indices;
- A-to-B and B-to-A propagation;
- every legal progression step;
- WAIT and RELAY states at the first and last progression steps;
- idle and annihilated paths;
- simultaneous four-path summation;
- repeated movement through all five electrodes three times without table
  generation or process restart;
- null state, network, and output pointers;
- uninitialised state and network;
- invalid electrode coordinates;
- wrong path delay and gap;
- wrong path-to-cell topology; and
- an out-of-range progression step.

The exact number of progression samples per path and direction was 5, 10, 20,
50, and 100 for 200, 100, 50, 20, and 10 ms respectively.

## ICC intrinsic-period accuracy

The ICC/path/network host suite was compiled and run at every supported
timestep with:

```bash
./tools/run_icc_host_tests.sh
```

All five runs passed. Measured Q1-to-Q1 periods were:

| Timestep | 15 s | 20 s | 23 s | 26 s | 30 s | 40 s |
|---:|---:|---:|---:|---:|---:|---:|
| 200 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40000 |
| 100 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40000 |
| 50 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40000 |
| 20 ms | 15000 | 20000 | 23000 | 26000 | 30000 | 40020 |
| 10 ms | 15000 | 20000 | 23010 | 25990 | 30010 | 40020 |

Values are milliseconds. The maximum observed intrinsic-period error was zero
at 200, 100, and 50 ms and 20 ms at 20 and 10 ms. The timestep-specific integer
calibration constants are defined in `inc/icc_calibration.h`.

## Path movement by timestep

For a 6 mm path traversed in 1000 ms, the propagation velocity is 6 mm/s. The
integer movement per network update is:

| Timestep | Movement | 60 um LUT-grid units | Steps per path |
|---:|---:|---:|---:|
| 200 ms | 1200 um | 20 | 5 |
| 100 ms | 600 um | 10 | 10 |
| 50 ms | 300 um | 5 | 20 |
| 20 ms | 120 um | 2 | 50 |
| 10 ms | 60 um | 1 | 100 |

The runtime matrix checked every progression position for every electrode,
path, and propagation direction. The relative-potential LUT itself is
independent of timestep; the compile-time integer dipole stride changes.

## Q1 and EGM alignment

An isolated wave was propagated through the real ICC/path/network update. For
Cells 1-4, A-to-B propagation began at Cell 1. Cell 5 used B-to-A propagation
beginning at Cell 5, ensuring that every electrode had a physically available
outgoing dipole and negative EGM feature. The test identified Q1 transitions
and searched the following path interval for the minimum EGM value.

Every measured Q1 time, minimum time, offset, sign, and value is asserted.
The complete 25-row result is stored in
`validation/egm_relative/alignment_summary.csv`.

| Timestep | Edge-cell offset | Interior-cell offset | Edge minimum | Interior minimum |
|---:|---:|---:|---:|---:|
| 200 ms | 200 ms | 200 ms | -61,394,733 | -61,394,733 |
| 100 ms | 100 ms | 100 ms | -79,444,198 | -79,444,198 |
| 50 ms | 100 ms | 100 ms | -79,444,198 | -79,444,198 |
| 20 ms | 100 ms | 100 ms | -79,444,198 | -79,444,198 |
| 10 ms | 110 ms | 110 ms | -79,530,905 | -79,530,905 |

The FlexPRET active-state predicate matches ICCNet Core: Q1, Q2, and Q3 are
active propagation states. During a relay handoff the preceding path therefore
enters annihilation while the source is refractory, preventing an artificial
reverse dipole. Each isolated outgoing negative feature is consequently one
relative-potential LUT contribution. The signed 32-bit overflow proof still
covers legal simultaneous contributions from independent paths.

## Verilator matrix

Command:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/run_egm_verilator_tests.sh
```

The script configured, cross-compiled, and executed 25 finite FlexPRET
Verilator cases: five electrode positions at each of five timesteps. Scenarios
12 and 13 provide isolated A-to-B and B-to-A waves. Each run verified all five
Q1 times at exact 1000 ms path intervals, the selected electrode coordinate,
the expected negative minimum and time offset, and the `DONE` marker.

Functional runs emitted every EGM sample. Separate no-EGM-trace runs measured
timing so per-sample printing did not affect release lateness. The scheduling
period was accelerated to `1,000,000 ns`; the biological timestep used by the
model remained 200, 100, 50, 20, or 10 ms.

| Biological timestep | Worst execution | Maximum lateness | Deadline margin |
|---:|---:|---:|---:|
| 200 ms | 25,400 ns | 160 ns | 199,974,600 ns |
| 100 ms | 25,420 ns | 160 ns | 99,974,580 ns |
| 50 ms | 25,400 ns | 160 ns | 49,974,600 ns |
| 20 ms | 25,380 ns | 160 ns | 19,974,620 ns |
| 10 ms | 25,360 ns | 160 ns | 9,974,640 ns |

These values include the ICC update, four path updates, and EGM computation
between two `rdtime()` observations. They exclude subsequent trace printing.
They are cycle-accurate simulation observations, not formal WCET bounds and not
DE1-SoC measurements.

Evidence:

```text
validation/egm_relative/verilator_summary.csv
validation/egm_relative/verilator_200ms_cell1_trace.csv
validation/egm_relative/verilator_10ms_cell5_trace.csv
```

## FPGA cross-build and SPM

Command:

```bash
./tools/run_egm_fpga_checks.sh
```

All five FPGA targets built successfully. The script inspected the ELF, EGM
object, table symbol, section sizes, symbols, and disassembly.

| Timestep | ISPM used | DSPM static | Stack reserved | Total used/reserved | Remaining of 128 KiB |
|---:|---:|---:|---:|---:|---:|
| 200 ms | 15,896 | 6,272 | 2,048 | 24,216 | 106,856 |
| 100 ms | 15,904 | 6,272 | 2,048 | 24,224 | 106,848 |
| 50 ms | 15,888 | 6,272 | 2,048 | 24,208 | 106,864 |
| 20 ms | 15,868 | 6,272 | 2,048 | 24,188 | 106,884 |
| 10 ms | 15,856 | 6,272 | 2,048 | 24,176 | 106,896 |

The configured regions are 64 KiB ISPM and 64 KiB DSPM. The 2 KiB stack is
reserved at the end of DSPM. The linker puts read-only data in `.data` with an
ISPM load image and DSPM runtime address; consequently the 3,204-byte LUT
contributes to both regions even though the table symbol is exactly 3,204
bytes.

## Runtime instruction audit

For every timestep, the EGM object contained no:

- RISC-V floating-point instruction;
- division or modulo instruction;
- square-root instruction;
- floating-point conversion helper;
- division or modulo helper;
- square-root helper; or
- 64-bit multiplication, division, or modulo helper.

The complete linked ELF contained four 32-bit integer helpers:
`__divsi3`, `__udivsi3`, `__modsi3`, and `__umodsi3`. Disassembly shows calls
outside EGM: `icc_network_1d_init()` validates that a path delay is divisible by
the timestep, and the integer formatting library divides values into decimal
digits. The production EGM object has no undefined helper reference. No 64-bit
arithmetic, floating conversion, or square-root helper was linked.

The 200 ms EGM-object disassembly is stored at
`validation/egm_relative/egm_200ms_disassembly.txt`.

## Limitations

- Execution measurements are from Verilator and do not replace formal WCET.
- Board loading, UART acquisition, and DE1-SoC timing were not performed.
- The physical output remains an uncalibrated model-potential unit.
- Runtime geometry is limited to the five-cell, 6 mm, 1000 ms configuration.
