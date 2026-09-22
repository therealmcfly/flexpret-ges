# Simultaneous Multi-Electrode EGM Design

## Status

Implemented on the `icc-model` branch. Software validation is recorded only for
tests actually rerun; the multi-electrode mode has not yet been flashed or
measured on the DE1-SoC board.

## Scope

The optional `all` mode computes five EGM channels from one ICC network state
on every model step:

| Channel | Electrode coordinate |
|---|---:|
| `egm_cell_1_scaled` | 0 um |
| `egm_cell_2_scaled` | 6000 um |
| `egm_cell_3_scaled` | 12000 um |
| `egm_cell_4_scaled` | 18000 um |
| `egm_cell_5_scaled` | 24000 um |

All channels reuse the single 801-entry, 3,204-byte relative-potential LUT.
There are no electrode-specific generated tables.

This feature does not change the EGM equation, scale, range, 60 um LUT
resolution, fixed five-cell/four-path topology, 6 mm gaps, 1000 ms delays, or
Cell 5 terminal-boundary behaviour. Arbitrary non-cell-aligned electrodes are
not supported.

## Build modes

The CMake setting is:

```text
ICC_EGM_OUTPUT_MODE=single
ICC_EGM_OUTPUT_MODE=all
```

`single` is the default and preserves the existing selected-electrode
behaviour and CSV format. Any other value is rejected during configuration.

In `all` mode, all five channels are computed. `ICC_EGM_ELECTRODE_X_UM` still
selects the pacing-lead cell and the backward-compatible channel sent to GES on
UART2. This avoids changing the current GES protocol. The emulator and finite
Verilator harness expose all five simultaneous channels. Sending all five EGM
channels to the future ICCNet controller requires a separate telemetry-protocol
change.

## Application state

`IccEgmBank` owns five `IccEgm` coordinate objects and an initialization flag:

```c
typedef struct {
    IccEgm channels[ICC_EGM_CHANNEL_COUNT];
    bool initialized;
} IccEgmBank;
```

`ICC_EGM_CHANNEL_COUNT` is tied to `ICC_NETWORK_1D_CELL_COUNT`, and a static
assert keeps this implementation fixed at five channels. The bank stores only
coordinates and flags. The LUT remains the single generated object used by
`icc_egm_compute()`.

## Initialization and coordinate mapping

`icc_egm_bank_init()` initializes the fixed coordinates once. It returns false
for a null bank or if any channel initialization fails.

`icc_egm_bank_channel_for_x_um()` maps a valid cell-centred coordinate to its
channel index. Invalid, negative, and out-of-range coordinates are rejected;
they are not rounded, clamped, or masked.

## Per-timestep computation

`icc_model_app_step_all()` performs exactly one network step and then calls
`icc_egm_bank_compute()` for the resulting state. The bank computes into a
temporary five-element array and copies to the caller only after all five
computations succeed. A failed channel therefore cannot publish a mixture of
new and stale results.

The order is:

```text
receive pacing input
wait for the release time
step the ICC network once
compute all five EGM channels from that state
publish the completed result set
```

`icc_egm_compute()` is read-only with respect to the network, so channel order
does not affect the network or another channel.

## Output formats

Single-mode emulator output retains `egm_scaled`. All-mode emulator output
replaces that field with:

```text
egm_cell_1_scaled,egm_cell_2_scaled,egm_cell_3_scaled,egm_cell_4_scaled,egm_cell_5_scaled
```

The finite harness emits:

```text
EGM_ALL,sample,time_ms,cell_1,cell_2,cell_3,cell_4,cell_5
```

The FPGA UART2 EGM frame remains `AA 55` plus one little-endian `int16` value.
All-mode computation does not silently alter that established GES interface.

## Cell 5 boundary semantics

During natural left-to-right propagation, Cell 5 has an incoming path but no
outgoing path. Its EGM channel returns to zero when the final incoming path ends
at Cell 5 Q1. Simultaneous output exposes this existing boundary behaviour; it
does not create a missing fifth path.

## Validation contract

Host tests must cover every supported timestep, all five electrodes, all four
paths, both directions, and every legal progression step. Each bank value must
equal the public single-electrode API for the same network state. Tests must
also prove no network mutation and all-or-nothing failure behaviour.

Finite Verilator tests must emit all five values in one run, verify expected Q1
spacing, compare each channel with its corresponding single-mode trace, cover
200, 100, 50, 20, and 10 ms, and retain separate functional and no-trace timing
runs.

FPGA and ELF checks must cross-build both modes at every timestep, confirm one
3,204-byte LUT, inspect EGM objects for forbidden floating-point, division,
modulo, square-root, allocation, and unexpected arithmetic helpers, and record
exact memory use and observed timing margins.

Physical board timing and telemetry are separate evidence. They must not be
claimed until the all-channel build is explicitly flashed and measured.
