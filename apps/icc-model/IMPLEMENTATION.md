# FlexPRET-Specific ICC Implementation

## 1. Purpose and scope

This document describes the first stage of porting the ICC model to FlexPRET.
The application implements one ICC cell as a deterministic, fixed-timestep
state machine that runs on either the FlexPRET FPGA or the Verilator-based
FlexPRET emulator.

This stage intentionally excludes propagation paths, an ICC network, EGM
generation, controller communication, and GES integration. Those components
will be added and validated separately.

## 2. Application structure

```text
icc-model/
├── CMakeLists.txt       Cross-build and target selection
├── README.md            Build, emulator, flash, and test commands
├── IMPLEMENTATION.md    Design rationale and verification record
├── inc/icc.h            Types, states, constants, and public interface
├── src/icc.c            Integer ICC state machine
├── src/main.c           FlexPRET periodic execution and CSV output
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
-0.004494 mV  -> -4494 nV
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
-67000000 nV + -13996 nV = -67013996 nV
-67013996 nV + -13996 nV = -67027992 nV
-67027992 nV + -13996 nV = -67041988 nV
```

In millivolts, these results are `-67.013996`, `-67.027992`, and
`-67.041988 mV`. No floating-point remainder is calculated at runtime.

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

Per-step increments were generated for the fixed 200 ms model period:

| State | Integer increment | Millivolts per step |
|---|---:|---:|
| Q1 | `+8704960 nV` | +8.704960 mV |
| Q2 | `-181952 nV` | -0.181952 mV |
| Q3 | `-1727227 nV` | -1.727227 mV |

Resting increments are selected from a lookup table:

| Pacemaker interval | Integer increment | Millivolts per step |
|---:|---:|---:|
| 15 s | `-30479 nV` | -0.030479 mV |
| 20 s | `-13996 nV` | -0.013996 mV |
| 23 s | `-10593 nV` | -0.010593 mV |
| 26 s | `-8967 nV` | -0.008967 mV |
| 30 s | `-6800 nV` | -0.006800 mV |
| 40 s | `-4494 nV` | -0.004494 mV |
| 0 or -1 | `0 nV` | Follower or blocked cell |

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

The initial wait is 4999 ms, while the timestep is 200 ms. Its first
representable expiration is consequently 5000 ms. On that step, the reset
voltage is assigned, Q0 is entered, and the Q0 increment is applied.

The `relay` field is retained for the future path implementation. A follower
cell uses a zero resting increment and requires a relay to enter Q1. A blocked
cell consumes a relay without depolarizing. No path currently produces relay
events.

## 6. FlexPRET periodic execution

`main.c` releases the cell update every 200 ms:

```c
uint32_t next_release = rdtime() + ICC_PERIOD_NS;

while (1) {
    fp_delay_until(next_release);
    next_release += ICC_PERIOD_NS;
    icc_step(&cell);
}
```

Using an absolute release sequence avoids intentionally adding the previous
iteration's execution time to the next period. CSV output is retained for
development and comparison but must later be removed, reduced, or included in
timing analysis.

The CSV format is:

```text
sample,time_ms,state,voltage_nv,nearest_uv
```

The header and rows use `printf()` so both appear through the Verilator host
output mechanism.

## 7. FPGA and emulator targets

The CMake `TARGET` option accepts `fpga` or `emulator`. Separate build
directories are required because the FPGA uses the SDK bootloader linker
configuration and the emulator uses the no-bootloader configuration.

| Target | Build directory | Launcher |
|---|---|---|
| FPGA | `build-fpga` | `bin/icc-model` |
| Emulator | `build-emu` | `bin/icc-model-emu` |

The exact commands are maintained in `README.md`.

## 8. Path numerical policy

The path model has not yet been implemented. When added, it will also use one
integer per stored physical quantity. It will not reuse the voltage unit for
unrelated quantities. Proposed explicit units include:

```text
propagation delay    -> integer ticks or microseconds
path length          -> integer micrometres
conduction velocity  -> integer micrometres per second
propagation position -> separately documented integer unit if required
```

Configuration-time calculations may require 64-bit intermediates even when a
stored path value is 32-bit. The authoritative path semantics and EGM needs
must be examined before selecting final path fields. EGM arithmetic remains a
separate numerical design because it introduces geometry, multiplication,
dynamic range, and accumulation.

## 9. Verification

Host tests cover the initial wait, first upstroke, follower relay behaviour,
blocked-cell behaviour, exact nanovolt accumulation, and diagnostic rounding.
Both FPGA and emulator targets build successfully, and the RISC-V binary is
checked for accidental software floating-point helper symbols.

The Verilator trace must preserve the previous physical results. Representative
values are:

| Sample | Time | State | Voltage |
|---:|---:|---|---:|
| 25 | 5000 ms | Q0 | `-67647596 nV` |
| 26 | 5200 ms | Q1 | `-58942636 nV` |
| 32 | 6400 ms | Q2 | `-24291052 nV` |
| 59 | 11800 ms | Q3 | `-30749031 nV` |
| 80 | 16000 ms | Q3 floor | `-67000000 nV` |
| 81 | 16200 ms | Q0 | `-67013996 nV` |

## 10. Current limitations and next stage

The application still contains one compile-time configured cell and diagnostic
output. It has no paths, network scheduler, EGM, controller protocol, or GES
integration. The next implementation stage is one directed path connecting a
pacemaker cell to a follower cell, followed by host and Verilator comparison
against the authoritative model.
