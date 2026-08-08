# FlexPRET-Specific ICC Implementation

## 1. Purpose and scope

This document describes the ICC, path, and 1D network stages of porting the
model to FlexPRET. The application implements five ICC cells and four
bidirectional paths as deterministic, fixed-timestep state machines that run on
either the FlexPRET FPGA or the Verilator-based FlexPRET emulator.

This stage intentionally excludes a two-dimensional network, EGM generation,
and GES integration. Those components will be added
and validated separately.

## 2. Application structure

```text
icc-model/
├── CMakeLists.txt       Cross-build and target selection
├── README.md            Build, emulator, flash, and test commands
├── IMPLEMENTATION.md    Design rationale and verification record
├── inc/icc.h            Types, states, constants, and public interface
├── inc/path.h           Integer path types, states, and public interface
├── inc/network.h        Static five-cell 1D network interface
├── src/icc.c            Integer ICC state machine
├── src/path.c           Integer bidirectional propagation state machine
├── src/network.c        Five-cell update ordering and path construction
├── src/main.c           Five-cell scheduling and target-specific output
└── tests/test_icc.c     Host-side model tests
```

The model in `icc.c` is platform independent. FlexPRET timing and output are
confined to `main.c`, allowing the same cell calculation to be tested with a
host compiler before it is cross-compiled for RISC-V.

## 3. Integer-nanovolt representation

### 3.1 Definition

The reference model expresses ICC membrane potential in millivolts. Before
compilation, voltages and per-step increments are converted into integer
nanovolts:

```text
VnV = VmV × 1,000,000
VmV = VnV / 1,000,000
```

The program represents a voltage with one signed 32-bit integer:

```c
#define ICC_NV_PER_UV 1000
#define ICC_NV_PER_MV 1000000
typedef int32_t IccVoltageNv;
```

Examples:

```text
-0.004401 mV  -> -4401 nV
-67.633600 mV -> -67633600 nV
```

This is a decimal fixed-point representation whose explicit storage unit is
the nanovolt. Describing the unit directly avoids treating the value as an
abstract integer with an unexplained scale factor.

### 3.2 Rationale

The selected FlexPRET configuration does not provide hardware floating-point
execution. Software floating point would increase code size and instruction
count and would complicate worst-case timing analysis. The ICC update requires
only voltage addition, comparison, and clamping after slopes have been
converted into per-step increments, so integer nanovolts provide all required
runtime operations.

An earlier prototype stored a signed whole-microvolt field and a separate
normalized fractional remainder. That form was mathematically correct but
required two-field addition, carry normalization, lexicographic comparison,
and a representation invariant. Storing the same value directly in nanovolts
allows native operations:

```c
cell->voltage_nv += increment_nv;

if (cell->voltage_nv < threshold_nv) {
    /* transition */
}
```

Fractional microvolt information remains present because one microvolt contains
1000 nanovolts. For example:

```text
-67000000 nV + -14307 nV = -67014307 nV
-67014307 nV + -14307 nV = -67028614 nV
-67028614 nV + -14307 nV = -67042921 nV
```

In millivolts, these results are `-67.014307`, `-67.028614`, and
`-67.042921 mV`. No floating-point remainder is calculated at runtime.

### 3.3 Resolution, accuracy, and range

The integer spacing is one nanovolt:

```text
1 nV = 0.001 µV = 0.000001 mV
```

This is the numerical resolution of the representation. It does not imply that
the physiological model or its parameters are accurate to one nanovolt.

Any quantization occurs when a reference value is converted to an integer
number of nanovolts. Once encoded, addition and comparison are exact in the
integer-nanovolt domain, subject to overflow.

A signed 32-bit nanovolt value covers approximately `-2.147 V` to `+2.147 V`.
The ICC cell operates around `-67 mV` to `-24 mV`, providing substantial
headroom. Future configuration inputs must nevertheless be range-checked.

## 4. Compiled voltage constants

Thresholds and reset values are stored directly in nanovolts:

| Quantity | Stored value | Original millivolts |
|---|---:|---:|
| Reset voltage | `-67633600 nV` | -67.633600 mV |
| Q0 to Q1 | `-67633900 nV` | -67.633900 mV |
| Q1 to Q2 | `-24109100 nV` | -24.109100 mV |
| Q2 to Q3 | `-28989400 nV` | -28.989400 mV |
| Q3 to Q0 | `-66988400 nV` | -66.988400 mV |
| Voltage floor | `-67000000 nV` | -67.000000 mV |

The source increments were calibrated for the production 200 ms model period:

| State | Integer increment | Millivolts per step |
|---|---:|---:|
| Q1 | `+8704960 nV` | +8.704960 mV |
| Q2 | `-181952 nV` | -0.181952 mV |
| Q3 | `-1727227 nV` | -1.727227 mV |

Resting increments are selected from the timestep-specific lookup table in
`inc/icc_calibration.h`. The production 200 ms values are:

| Pacemaker interval | Integer increment | Millivolts per step |
|---:|---:|---:|
| 15 s | `-32003 nV` | -0.032003 mV |
| 20 s | `-14307 nV` | -0.014307 mV |
| 23 s | `-10593 nV` | -0.010593 mV |
| 26 s | `-8489 nV` | -0.008489 mV |
| 30 s | `-6728 nV` | -0.006728 mV |
| 40 s | `-4401 nV` | -0.004401 mV |
| 0 or -1 | `0 nV` | No intrinsic activity or blocked cell |

For 100, 50, 20, and 10 ms, an exhaustive search selects the constant integer
Q0 increment that minimizes absolute Q1-to-Q1 period error. The runtime state
update remains a single integer addition. All target periods are exact at 100
and 50 ms; the maximum residual error at 20 and 10 ms is 20 ms. The measured
values and search limitations are reported in
[TIMESTEP_TEST_RESULTS.md](TIMESTEP_TEST_RESULTS.md).


The target state update performs no floating-point operation, slope
multiplication, division, or square root. The diagnostic nearest-microvolt
function divides integer nanovolts by 1000 only for CSV output; its result is
never fed back into the model.

## 5. ICC state and update ordering

```c
typedef struct {
    IccState state;
    IccVoltageNv voltage_nv;
    uint32_t wait_ms_accum;
    int8_t pacemaker_interval_s;
    int8_t relay;
    bool initialized;
} Icc;
```

The states are `WAIT`, Q0 resting, Q1 upstroke, Q2 plateau, and Q3
repolarization. Each `icc_step()` call first evaluates transitions and then
applies the nanovolt increment for the resulting state. A newly entered state
therefore applies its increment in the same update.

The initial wait is 4999 ms. At the production 200 ms timestep, its first
representable expiration is 5000 ms. On that step, the reset voltage is
assigned, Q0 is entered, and the Q0 increment is applied. Smaller timesteps use
the same elapsed-millisecond accumulator.

The `relay` field connects the cell and path state machines. A cell configured
with zero has no intrinsic activity but can enter Q1 in response to a relay. A
blocked cell consumes a relay without depolarizing. `icc_is_active()` and
`icc_stimulate()` provide the path with a small interface instead of duplicating
ICC internals.

## 6. FlexPRET periodic execution

`main.c` releases the complete 1D network update at the selected model period.
The production default is 200 ms:

```c
uint32_t next_release = rdtime() + ICC_PERIOD_NS;

while (1) {
    fp_delay_until(next_release);
    next_release += ICC_PERIOD_NS;
    icc_network_1d_step(&network);
}
```

Using an absolute release sequence avoids intentionally adding the previous
iteration's execution time to the next period. The ordinary emulator retains
CSV output for development and comparison. The autonomous FPGA build does not
transmit runtime telemetry.

The emulator CSV format is:

```text
sample,time_ms,fpga_time_ns,period_ns,release_lateness_ns,cell_0_state,cell_0_nv,...,cell_4_state,cell_4_nv,path_0_state,...,path_3_state
```

The header and rows use `printf()` so both appear through the Verilator host
output mechanism.

## 7. FPGA and emulator targets

The CMake `TARGET` option accepts `fpga` or `emulator`.
`ICC_MODEL_TIMESTEP_MS` defaults to 200 and must divide 200 exactly; 100, 50,
20, and 10 ms are available for experimental sensitivity testing. Separate
build directories are required because the FPGA uses the SDK bootloader linker
configuration and the emulator uses the no-bootloader configuration.

| Target | Build directory | Launcher |
|---|---|---|
| FPGA | `build-fpga` | `bin/icc-model` |
| Emulator | `build-emu` | `bin/icc-model-emu` |

The exact commands are maintained in `README.md`.

## 8. Path numerical policy

The path model preserves the six-state `iccnet-core` propagation semantics:

```text
IDLE
ANNIHILATE
CELL_A_WAIT
CELL_A_RELAY
CELL_B_WAIT
CELL_B_RELAY
```

A path connects two `Icc` objects and stores its propagation delay and elapsed
time as integer milliseconds:

```c
typedef struct {
    IccPathState state;
    uint32_t elapsed_ms;
    int32_t active_time_ms[2];
    uint16_t delay_ms;
    uint8_t gap_mm;
    Icc *cell_a;
    Icc *cell_b;
    bool initialized;
} IccPath;
```

`active_time_ms[0]` represents A-to-B propagation and
`active_time_ms[1]` represents B-to-A propagation. The value `-1` denotes an
inactive direction. This replaces the two external `float` times in seconds
used by `iccnet-core`. The physical gap remains an integer number of
millimetres and is stored for the later EGM stage; it does not participate in
the current relay calculation.

When one endpoint is active, the path enters the corresponding wait state. It
adds `ICC_TIMESTEP_MS` on each update and sets the destination's relay before
the update on which the nominal delay expires. The destination consumes that
relay during the next ICC step. For a 1000 ms delay and a 200 ms timestep:

```text
A enters Q1 and path detects A       0 ms
path A_WAIT                         200 ms
path A_WAIT                         400 ms
path A_WAIT                         600 ms
path sets B relay                   800 ms
B consumes relay and enters Q1     1000 ms
```

Waiting until the path accumulator itself reached 1000 ms would delay the ICC
transition until 1200 ms because the cell updates occur before the path update.
The one-step look-ahead therefore preserves the effective propagation delay of
`iccnet-core`.

If both endpoints are active while the path is idle, the path enters
`ANNIHILATE` and relays neither wave. It returns to `IDLE` when both endpoints
leave Q1. The state machine is bidirectional, so an active destination can be
detected in the reverse direction after a relay. This matches `iccnet-core`;
the source cell ignores that reverse relay while outside Q0.

The path calculation contains no floating-point operations, multiplication,
division, square root, or dynamic allocation. EGM arithmetic remains a
separate numerical design because it introduces geometry, distance-dependent
terms, multiplication, dynamic range, and accumulation.

### 8.1 Static 1D network

The network uses fixed-size arrays and performs no dynamic allocation:

```c
typedef struct {
    Icc cells[5];
    IccPath paths[4];
    bool initialized;
} IccNetwork1d;
```

Path `i` connects Cell `i` to Cell `i + 1`. Each step updates all five cells
first and then all four paths, preserving the execution order used by
`iccnet-core` applications. Initialization now accepts arrays containing five
independent cell intervals, four delays, and four gaps. Both the emulator and
FPGA targets use the same compiled configuration: Cell 4 is a 20-second
pacemaker, Cells 0-3 are followers, every path delay is 1000 ms, and every gap
is 6 mm. No floating-point conversion is performed by the FPGA.

## 9. Verification

Host tests cover the original ICC and path behaviour, network topology,
invalid pacemaker configuration, and the first activation time of every cell
in the chain. Both FPGA and emulator targets build successfully, and the
RISC-V binary is checked for accidental software floating-point helper
symbols.

The Verilator trace confirms one 1000 ms delay per hop:

| Cell | First Q1 time | Delay from preceding cell |
|---:|---:|---:|
| 4 | 5200 ms | Pacemaker |
| 3 | 6200 ms | 1000 ms |
| 2 | 7200 ms | 1000 ms |
| 1 | 8200 ms | 1000 ms |
| 0 | 9200 ms | 1000 ms |

Across this captured propagation window, the measured period remained
`200000000 ns` and release lateness remained `160 ns`.

The RISC-V memory use for the five-cell network is:

| Target | ROM | RAM |
|---|---:|---:|
| Emulator | 12,732 bytes | 3,548 bytes |
| FPGA | 11,408 bytes | 1,096 bytes |

## 10. Current limitations and next stage

The application contains an autonomous five-cell 1D network with compiled
parameters. Runtime configuration, telemetry, grid dimensions, EGM, a
two-dimensional network, and GES integration are not implemented. The next
numerical stage is the EGM calculation, which should be designed and validated
separately from the integer ICC and path state machines.

The 200 ms timestep is the validated numerical mode. Experimental smaller
timesteps meet the five-cell Verilator deadlines and preserve exact integer path
delays, but timestep-specific Q0 calibration limits measured intrinsic-period
error to 20 ms. Exact smaller-timestep cpm would require additional state,
such as fractional remainder accumulation or explicit time-based phase logic.
