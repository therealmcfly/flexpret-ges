# FlexPRET ICC, Path, Network, and EGM Implementation

## Scope

`icc-model` is an autonomous FlexPRET application containing five ICC cells,
four bidirectional one-dimensional paths, and a moving-dipole EGM output. The
same C state machines are used by host tests, the Verilator target, and the
FPGA cross-build.

All target-side biological and EGM calculations use fixed-size integer state.
There is no dynamic allocation or runtime floating point.

## Source structure

```text
inc/icc.h                  ICC state and nanovolt API
inc/icc_calibration.h      timestep-specific ICC increments
inc/path.h                 bidirectional path state
inc/network.h              fixed five-cell topology
inc/egm.h                  runtime electrode and EGM API
inc/app.h                  shared application state and step API
src/icc.c                  ICC state machine
src/path.c                 propagation state machine
src/network.c              cell-then-path update order
src/egm.c                  relative-coordinate LUT indexing and sum
src/app.c                  shared network/EGM initialization and update
src/main.c                 minimal FPGA production scheduler
src/emulator_main.c        continuous emulator CSV output
src/verilator_test_main.c  finite scenario and timing harness
tools/generate_egm_lut.c   host double-precision LUT generator
```

CMake selects exactly one entry point. FPGA builds compile `main.c`; ordinary
emulator builds compile `emulator_main.c`; emulator builds configured with
`ICC_VERILATOR_TEST_SCENARIO` compile `verilator_test_main.c`. Consequently,
the FPGA translation unit contains no scenario tables, forced-state setup,
trace formatting, or finite-test termination code.

## ICC voltage representation

The reference cell voltage in millivolts is compiled into signed integer
nanovolts:

```text
integer_nanovolts = millivolts * 1,000,000
```

`IccVoltageNv` is `int32_t`. Its one-nanovolt resolution retains the required
fractional microvolt increments while allowing native addition and comparison.
The physiological range around -67 to -24 mV is far inside signed 32-bit range.

Thresholds, resets, and timestep-specific increments are compile-time integer
constants. Supported biological timesteps are 200, 100, 50, 20, and 10 ms.
`inc/icc_calibration.h` selects the corresponding resting increment. ICC state
updates perform addition, comparison, and clamping; no floating point or square
root is used.

## ICC state machine

Each `Icc` stores state, voltage, elapsed waiting time, intrinsic interval,
relay input, and initialization state. States are:

```text
WAIT -> Q0_RESTING -> Q1_UPSTROKE -> Q2_PLATEAU -> Q3_REPOLARIZATION
```

A positive supported interval provides intrinsic pacing. Interval zero is a
functional follower that requires a relay. Interval -1 represents a blocked
cell that consumes a relay without entering Q1.

Each `icc_step()` evaluates transitions and then applies the new state's
integer voltage increment. `icc_is_active()` and `icc_stimulate()` form the path
interface.

## Path and network implementation

Each `IccPath` has fixed endpoint pointers, millisecond delay, integer gap,
elapsed time, progression step, and one of six states:

```text
IDLE
ANNIHILATE
CELL_A_WAIT
CELL_A_RELAY
CELL_B_WAIT
CELL_B_RELAY
```

The destination relay is raised one model step before the path accumulator
reaches its nominal delay because network ordering updates cells before paths.
The destination consumes the relay on the following network step, preserving
the configured physical activation delay.

`IccNetwork1d` contains statically allocated arrays of five cells and four
paths. Path `i` connects Cell `i` to Cell `i+1`. Every network call updates all
cells first and all paths second.

The general ICC/path tests retain configurable integer gaps and delays. The EGM
runtime validates the specific 6 mm, 1000 ms geometry required by its
division-free coordinate mapping.

## Relative-potential EGM table

The table coordinate is:

```text
oriented_relative_um = direction * (electrode_x_um - dipole_x_um)
```

For the straight 1D model, fixed height reduces the moving-dipole equation to a
single spatial variable. The generator evaluates the equation in double
precision for positions `-24000..24000 um` at 60 um spacing, rounds to a scale
of 10,000,000, and emits one 801-entry `int32_t` array.

The host generator also emits a CSV, metadata report, and SHA-256 manifest.
Generated files are ignored and must be recreated before configuring CMake.

## Runtime coordinates and indexing

The target represents distance in 60 um lattice units. Cell coordinates are
0, 100, 200, 300, and 400. A 6 mm path spans 100 units. The per-update dipole
strides are:

```text
200 ms -> 20 units
100 ms -> 10 units
 50 ms ->  5 units
 20 ms ->  2 units
 10 ms ->  1 unit
```

For an active path:

```text
A-to-B dipole = path_index*100 + progression*stride
B-to-A dipole = (path_index+1)*100 - progression*stride

A-to-B relative = electrode - dipole
B-to-A relative = dipole - electrode

lookup_index = relative + 400
```

Every operation is signed 32-bit addition, subtraction, multiplication by a
small compile-time value, comparison, or indexed load. No runtime division,
modulo, unit conversion, interpolation, floating point, or square root occurs.

## Electrode state and error handling

`IccEgm` stores the current electrode lattice coordinate explicitly. Its setter
accepts the five cell-centre x-positions in micrometres. It returns `false` for
all other values, including in-range values that are not supported electrode
locations. A rejected setting leaves the established electrode unchanged.

`icc_egm_compute()` returns `false` for invalid network topology, gap, delay,
progression, state, or pointer. Invalid configuration is not reported as a
physiological zero. An initialized valid network with no active path produces a
legitimate zero sample.

## Summation and overflow

At most four path contributions are summed. The generator calculates the
largest absolute entry and rejects the table if four such entries can exceed
`INT32_MAX`. The runtime repeats this condition as `_Static_assert`. The sum is
therefore safe in `int32_t`; an `int64_t` accumulator and its possible compiler
helpers are unnecessary.

## FlexPRET scheduling

The production application maintains an absolute nanosecond release sequence:

```c
next_release = rdtime() + ICC_PERIOD_NS;
while (1) {
    fp_delay_until(next_release);
    next_release += ICC_PERIOD_NS;
    icc_network_1d_step(&network);
    icc_egm_compute(&egm, &network, &egm_value);
}
```

The FPGA build runs autonomously and does not print each model sample. The
ordinary emulator prints CSV. Finite Verilator scenarios use the same model
functions, stop automatically, and optionally print one EGM row per step.
Execution-time instrumentation is compiled only for the emulator so it does
not alter the FPGA production loop.

## Target memory and instructions

At 100 ms, the largest measured FPGA build used 16,036 bytes of ISPM and 6,272
bytes of static DSPM, with a 2,048-byte reserved stack. Total SPM used or
reserved was 24,356 of 131,072 bytes. These figures were remeasured after the
entry-point separation on 2026-08-18.

The FlexPRET linker copies read-only `.data` from an ISPM load image to DSPM at
startup. The logical 3,204-byte table therefore consumes space in both regions.

Object-level `nm` and `objdump` inspection confirmed that `src/egm.c` contains
no floating-point, square-root, division, or modulo instruction and references
no arithmetic helper. Four 32-bit division/modulo helpers remain elsewhere in
the complete ELF for network configuration validation and linked integer
formatting. No 64-bit arithmetic helper is present.

## Validation

The reproducible commands and measured numerical, host-runtime, Verilator,
SPM, instruction, and alignment results are documented in:

- `EGM_LUT_VALIDATION.md`
- `EGM_RUNTIME_VALIDATION.md`
- `EGM_ALL_TIMESTEP_WAVEFORMS.md`

Machine-readable evidence is under `validation/egm_relative/`.

## Limitations

The target implements a straight 1D five-cell network, fixed 1 mm electrode
height, cell-centre electrode positions, and 6 mm/1000 ms EGM path geometry.
EGM output remains in model-potential units. A two-dimensional conductor,
clinical calibration, runtime arbitrary electrode coordinates, HEPTANE WCET,
and physical DE1-SoC validation are outside the current implementation.
