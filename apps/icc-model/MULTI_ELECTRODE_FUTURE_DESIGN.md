# Future Simultaneous Multi-Electrode EGM Design

## Status

This document is a proposal for a future modification. It is not implemented
by commit `81adda9`, and none of the validation results in this repository claim
that simultaneous multi-electrode output has been tested.

The current FPGA application creates one `IccEgm` object, selects one of the
five cell-centred electrode positions, computes one EGM value per model step,
and prints one `egm_scaled` column. The existing five-electrode waveform records
were produced by separate emulator runs, one selected electrode per run.

## Objective

Add an optional mode that computes five EGM channels from the same network
state on every timestep:

| Output channel | Electrode coordinate |
|---|---:|
| `egm_cell_1_scaled` | 0 um |
| `egm_cell_2_scaled` | 6000 um |
| `egm_cell_3_scaled` | 12000 um |
| `egm_cell_4_scaled` | 18000 um |
| `egm_cell_5_scaled` | 24000 um |

All channels must reuse the existing 801-entry relative-potential LUT. The
design must not generate or store an electrode-specific table.

## Non-goals

The first multi-electrode version should not:

- change the EGM equation, LUT scale, LUT range, or 60 um resolution;
- add arbitrary non-cell-aligned electrode coordinates;
- change the fixed five-cell, four-path, 6 mm, 1000 ms specialization;
- change the Cell 5 terminal-boundary behaviour;
- represent physical ADC inputs, electrode impedance, filtering, or noise;
- replace the current single-electrode mode before the new mode is validated.

## Recommended compatibility mode

Keep the present single-electrode behaviour as the default and add a CMake
selection such as:

```text
ICC_EGM_OUTPUT_MODE=single
ICC_EGM_OUTPUT_MODE=all
```

In `single` mode, retain `ICC_EGM_ELECTRODE_X_UM` and the existing CSV format.
In `all` mode, ignore the single startup coordinate, initialize all five fixed
coordinates, and emit five EGM columns. CMake should reject any other mode.

This staged approach preserves current board and test workflows while the
five-channel timing and telemetry cost is measured.

## Proposed application state

The smallest implementation can reuse the existing public `IccEgm` API:

```c
#define ICC_EGM_CHANNEL_COUNT ICC_NETWORK_1D_CELL_COUNT

static const int32_t kEgmElectrodePositionsUm[ICC_EGM_CHANNEL_COUNT] = {
    0,
    6000,
    12000,
    18000,
    24000
};

IccEgm electrodes[ICC_EGM_CHANNEL_COUNT];
IccEgmValue egm_values[ICC_EGM_CHANNEL_COUNT];
```

The array contains only electrode coordinates and initialization flags. The
LUT remains the single `kEgmRelativePotential` object in `src/egm.c`.

If the feature grows beyond this fixed application, the arrays and helper
functions may later be wrapped in an `IccEgmBank` type. That abstraction is not
required for the first implementation.

## Initialization

Initialize every channel once after the network is initialized:

```c
static bool initialize_egm_channels(
    IccEgm electrodes[ICC_EGM_CHANNEL_COUNT])
{
    for (uint8_t index = 0U;
         index < ICC_EGM_CHANNEL_COUNT;
         ++index) {
        if (!icc_egm_init(
                &electrodes[index],
                kEgmElectrodePositionsUm[index])) {
            return false;
        }
    }
    return true;
}
```

Initialization failure must stop the application rather than leave a partially
configured set of telemetry channels.

## Per-timestep computation

Step the ICC network exactly once, then compute every EGM channel before
modifying the network again:

```c
static bool compute_egm_channels(
    const IccEgm electrodes[ICC_EGM_CHANNEL_COUNT],
    const IccNetwork1d *network,
    IccEgmValue results[ICC_EGM_CHANNEL_COUNT])
{
    IccEgmValue next_results[ICC_EGM_CHANNEL_COUNT];

    for (uint8_t index = 0U;
         index < ICC_EGM_CHANNEL_COUNT;
         ++index) {
        if (!icc_egm_compute(
                &electrodes[index], network, &next_results[index])) {
            return false;
        }
    }

    for (uint8_t index = 0U;
         index < ICC_EGM_CHANNEL_COUNT;
         ++index) {
        results[index] = next_results[index];
    }
    return true;
}
```

Using temporary results gives all-or-nothing output: a failed channel cannot
mix a new value with stale values from other channels.

The main loop ordering should be:

```text
wait for release
measure release timing
step the network once
compute all five EGM channels from that network state
measure execution time
emit one telemetry row containing all five values
```

`icc_egm_compute()` is read-only with respect to the network, so channel order
must not affect the values.

## Telemetry format

The all-channel CSV should replace the ambiguous single `egm_scaled` column
with explicit fields:

```text
egm_cell_1_scaled,egm_cell_2_scaled,egm_cell_3_scaled,egm_cell_4_scaled,egm_cell_5_scaled
```

Keep the column order aligned with the existing cell voltage order. The test
trace format should use the same ordering.

Five decimal values increase UART formatting and transmission cost. Execution
timing and UART throughput must be measured separately. If serial output cannot
meet the required release period, possible later optimizations include reduced
telemetry frequency or a compact binary record. Those optimizations must not
silently drop model computations or change biological time.

## Cell 5 boundary semantics

Simultaneous output does not change the underlying path-based model. During
natural left-to-right propagation, Cell 5 has an incoming path but no outgoing
path. Its EGM channel therefore returns to zero when the final incoming path
ends at Cell 5 Q1. The multi-electrode feature will make this boundary behaviour
visible in the same row as Cells 1-4; it will not create a missing fifth path.

Changing that waveform would be a separate modelling decision requiring a new
specification and validation reference.

## Expected resource impact

- LUT storage remains exactly 3,204 bytes because all channels share it.
- Five `IccEgmValue` results require 20 bytes before alignment or surrounding
  structure overhead.
- The exact size of five `IccEgm` objects must be recorded with `sizeof` for the
  target ABI rather than inferred from the source fields.
- EGM computation is performed five times per model step instead of once.
- UART formatting and transmission add four EGM values per telemetry row.

Do not claim a five-times execution cost from inspection alone. Measure the
complete target and compare it with the single-channel baseline.

## Implementation sequence

1. Add and validate the `single`/`all` CMake mode while keeping `single` as the
   default.
2. Add fixed coordinate and channel-count definitions with compile-time checks
   tying the count to the five-cell network.
3. Add initialization and all-or-nothing computation helpers.
4. Extend emulator and ordinary CSV headers and rows only in `all` mode.
5. Add host equivalence tests before changing Verilator expectations.
6. Add finite all-channel Verilator traces and compare them with the existing
   single-channel results.
7. Cross-build every supported timestep and repeat the EGM ELF audit.
8. Measure execution time, scheduling, memory, and UART output on Verilator.
9. Flash and capture DE1-SoC output only after explicit approval.
10. Update the README and validation records only with tests actually rerun.

## Required host tests

At every supported timestep:

- initialize all five channels successfully;
- calculate all channels from the same network state;
- verify each simultaneous value equals the value produced by the existing
  single-electrode API for the same coordinate and state;
- cover every path, both propagation directions, and every legal progression
  step;
- confirm channel computation does not modify the network or another channel;
- confirm a failure does not publish partially updated results;
- confirm there is exactly one 801-entry, 3,204-byte LUT;
- retain all current invalid-input and configuration-mismatch tests.

## Required Verilator tests

- Emit all five EGM values in one finite run.
- Assert Q1 events at the expected 1000 ms spacing.
- Compare each all-channel series with a corresponding single-channel trace
  generated from the same scenario and timestep.
- Verify Cell 1 is zero before its intrinsic Q1 in the clean waveform window.
- Verify the documented Cell 5 terminal-boundary behaviour.
- Test all 200, 100, 50, 20, and 10 ms configurations.
- Retain separate functional-trace and no-trace timing runs.

## Required FPGA and ELF checks

For every supported timestep:

- cross-build the FPGA executable;
- record `.text`, `.data`, `.bss`, ISPM, DSPM, and stack reservation;
- verify `kEgmRelativePotential` remains exactly 3,204 bytes and appears once;
- inspect the EGM objects for floating-point, division, modulo, square-root,
  dynamic-allocation, and unexpected arithmetic helpers;
- measure worst observed execution time and deadline margin;
- measure or bound the larger UART record cost;
- compare all results with the current single-channel baseline.

## Completion criteria

The future feature is ready only when:

- single-electrode mode remains backward compatible;
- all five values are produced from one network step in one execution;
- simultaneous values match independently generated single-channel values;
- one shared LUT is proven in the ELF;
- all host, Verilator, FPGA, timing, memory, and telemetry checks pass;
- documentation distinguishes simulated evidence from physical board evidence;
- Cell 5 boundary behaviour is explicitly retained or separately redesigned.
