# FlexPRET EGM Design Plan

Status: the generator now emits all four path tables and the first FlexPRET
integer lookup-and-sum runtime is integrated and tested. One path was compared
directly with `iccnet-core`; all four paths use the same equation at their
translated network coordinates.

## 1. Purpose and scope

The first FlexPRET EGM stage will reproduce the moving-path-dipole EGM model
from `iccnet-core` for the existing autonomous five-cell 1D network. It will
support one electrode and biological timesteps of 200, 100, 50, 20, and 10 ms.

The initial scope is deliberately fixed:

- five ICC cells and four bidirectional paths;
- one electrode;
- one-dimensional geometry;
- uniform cell spacing;
- compile-time path delays, geometry, and EGM parameters; and
- one selected timestep per FPGA binary.

Multiple electrodes, a two-dimensional network, runtime geometry changes, and
arbitrarily varying path gaps are later stages.

### Implemented generator and runtime milestone

The generator is available in `tools/generate_egm_lut.c`. It generates both
directions of all four 6 mm, 1000 ms paths for one electrode at every supported
timestep. Generated headers, CSV files, the numerical report, and a checksum
manifest are recreated under `generated/egm_1d5c/`. This directory is ignored
by Git; the generator source and configuration are the reproducible source of
truth.

Run the reproducible generator test with:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/generate_egm_luts.sh
./tools/run_egm_lut_tests.sh
./tools/run_egm_runtime_tests.sh
```

The original path-0 floating samples matched the actual `iccnet-core`
implementation at all 370 tested path states. The runtime now selects and sums
the four path-specific tables. The numerical and target integration evidence
is recorded in [EGM_LUT_VALIDATION.md](EGM_LUT_VALIDATION.md) and
[EGM_RUNTIME_VALIDATION.md](EGM_RUNTIME_VALIDATION.md).

## 2. Reference `iccnet-core` EGM model

`iccnet-core` does not calculate EGM directly from ICC voltage. An active
propagation path creates a moving dipole. For every active path, the reference
implementation calculates the dipole position and its geometric relationship
to the electrode, then adds a contribution of the form

```text
potential = dipole_moment
          * (longitudinal_weight * along_path_distance
             - transverse_weight * perpendicular_distance)
          / distance^3
```

The reference implementation performs floating-point position calculations,
multiplication, division, and square roots during every update.

The default parameters are:

| Parameter | Value |
|---|---:|
| Dipole moment | 18.0 |
| Longitudinal weight | 1.0 |
| Transverse weight | 0.1 |

Its output is expressed in the model's existing scaled potential units, not a
declared physical voltage unit.

## 3. Why FlexPRET will use generated lookup tables

The path geometry, electrode position, electrode height, propagation direction,
path delay, timestep, and EGM parameters are fixed when the FPGA application is
built. A discretely updated path can therefore occupy only a finite set of
observable positions.

For example, a 1000 ms path at a 200 ms timestep exposes the propagation times
0, 200, 400, 600, and 800 ms. The reference model always produces the same EGM
contribution for a given path, direction, and progress step. Recalculating that
contribution on the target would repeat an expensive deterministic calculation.

The host-side generator will partially evaluate the reference equation before
deployment. It will calculate every reachable contribution using floating
point, convert the result to a scaled signed 32-bit integer, and emit a C lookup
table. The FlexPRET runtime will only select and add table entries.

This is a deliberate time-memory trade-off:

- expensive geometry is evaluated offline;
- target execution uses no floating point, square root, multiplication,
  division, or 64-bit arithmetic;
- the EGM execution path has fixed, easily bounded control flow; and
- numerical conversion error can be measured exhaustively before deployment.

The generator itself runs on the development computer. It is not included in
the FPGA binary and is not part of the target WCET.

## 4. Generator inputs and outputs

The generator must receive all parameters that affect the table:

- `ICC_MODEL_TIMESTEP_MS`;
- path delays;
- path geometry and uniform spacing;
- electrode position and height;
- dipole moment;
- longitudinal weight; and
- transverse weight.

It will emit:

1. a generated C header containing signed 32-bit lookup entries;
2. a CSV containing the floating reference value, stored integer value,
   reconstructed value, and error for every entry;
3. minimum and maximum values and accumulator overflow margin;
4. maximum absolute error and RMSE;
5. configuration metadata; and
6. a reproducibility identifier or hash for the input configuration.

Separate entries are required for each path and direction:

```text
path 0, A to B, progress 0..N-1
path 0, B to A, progress 0..N-1
...
path 3, B to A, progress 0..N-1
```

The generator must follow the reachable states of the FlexPRET path state
machine exactly. In particular, the current relay ordering normally exposes
times from zero through `delay_ms - timestep_ms`; it must not invent an endpoint
sample that the running path model never observes.

## 5. Integer representation

Every table entry will use a signed 32-bit type such as `IccEgmValue`. A single
common scale will be used across every supported timestep so results from
different timestep builds remain directly comparable.

The scale will be selected only after sweeping every table entry. It must
satisfy both accuracy and the accumulator bound:

```text
maximum simultaneous path sum < INT32_MAX
minimum simultaneous path sum > INT32_MIN
```

The stored value is

```text
stored_egm = round(reference_egm * EGM_SCALE)
```

The paper must distinguish the integer resolution created by `EGM_SCALE` from
the physiological accuracy of the underlying dipole model.

## 6. Runtime structure

The path model exposes a small integer progress index. It is set to zero
when propagation starts, incremented once per model update, and reset when the
path becomes inactive. This avoids calculating `elapsed_ms / timestep_ms` on
FlexPRET.

The first EGM implementation is manually unrolled:

```c
IccEgmValue egm_1d5c_compute(const IccNetwork1d *network)
{
    IccEgmValue result = 0;

    result += egm_path_0_contribution(&network->paths[0]);
    result += egm_path_1_contribution(&network->paths[1]);
    result += egm_path_2_contribution(&network->paths[2]);
    result += egm_path_3_contribution(&network->paths[3]);

    return result;
}
```

Each helper performs a bounded state/direction selection and one table access.
There is no EGM accumulation loop to annotate in HEPTANE.

The periodic update order is:

```text
update all ICC cells
update all paths
read each path direction and progress index
look up and sum the four EGM contributions
store or report the EGM sample
```

## 7. Supported timesteps and memory cost

The supported timesteps are 200, 100, 50, 20, and 10 ms. Only one timestep and
its matching table are linked into a particular FPGA binary.

For four paths, two directions, signed 32-bit entries, and a worst-case delay of
5000 ms on every path, the approximate size per electrode is:

| Timestep | Positions per direction | Table size |
|---:|---:|---:|
| 200 ms | 25 | 800 bytes |
| 100 ms | 50 | 1.6 KB |
| 50 ms | 100 | 3.2 KB |
| 20 ms | 250 | 8 KB |
| 10 ms | 500 | 16 KB |

The exact size is proportional to the sum of the configured path delays:

```text
bytes = 4 bytes * 2 directions
      * sum(delay_ms[path] / timestep_ms)
```

Table placement in FlexPRET scratchpad memory must be checked from the linker
map for every configuration, particularly at 10 ms and when adding electrodes.

## 8. Procedure when changing timestep

Changing timestep is a build-time configuration change. It is not permitted to
change the timestep at runtime while continuing to use the old table.

For a supported timestep change:

1. Select one of 200, 100, 50, 20, or 10 ms through
   `ICC_MODEL_TIMESTEP_MS`.
2. Confirm that every path delay is greater than the timestep and is an exact
   multiple of it.
3. Use the existing ICC resting-increment calibration entry for that timestep.
4. Run `./tools/generate_egm_luts.sh` to recreate all supported EGM lookup
   tables for the network configuration.
5. Build in a separate timestep-specific build directory to prevent stale
   objects or tables.
6. Run the host ICC, path, network, EGM, and reference-comparison tests.
7. Inspect the generated table report for quantization error and 32-bit
   overflow margin.
8. Build the FPGA and emulator targets.
9. Check the RISC-V binary for floating-point, square-root, multiplication,
   division, and 64-bit helper dependencies in the EGM path.
10. Run the timestep-specific Verilator deadline and output trace tests.
11. Run DE1-SoC timing validation for every timestep claimed in a result.
12. Perform and archive a separate HEPTANE analysis for that compiled binary.

The generated header must include metadata such as:

```c
#define EGM_LUT_TIMESTEP_MS 200U
#define EGM_LUT_CELL_COUNT 5U
#define EGM_LUT_PATH_COUNT 4U
```

Compilation must fail when the table metadata does not match
`ICC_TIMESTEP_MS`, the network dimensions, or the configured geometry. This
prevents accidentally testing one timestep with another timestep's table.

All five canonical tables are generated together, so changing only the
timestep selects the matching generated table automatically after generation.
The generation step is mandatory after cloning or pulling because the output
directory is not committed. It is also required after changing path delays,
spacing, electrode geometry, EGM parameters, the integer scale, or the
generator algorithm. `GENERATION_SHA256SUMS.txt` records the exact generator
source hash and output hashes used for a build or experiment.

Changing only cell intrinsic intervals or pacemaker locations does not require
a new EGM table. Those values control when propagation begins, not the
geometric contribution at a particular path position.

## 9. Differences from `iccnet-core`

| Aspect | `iccnet-core` | FlexPRET EGM |
|---|---|---|
| Arithmetic | Runtime floating point | Runtime signed 32-bit integers |
| Geometry | Calculated every update | Calculated offline |
| Square root and division | Runtime `sqrtf` and division | None at runtime |
| Dipole position | Runtime floating-point position | Discrete progress index |
| Path time | Floating-point seconds | Integer milliseconds and step index |
| Direction | Normalized at runtime | Separate precomputed A-to-B and B-to-A tables |
| Potential accumulation | Floating-point addition | Scaled integer addition |
| Configuration | Parameters can be passed at runtime | Geometry and parameters compiled into the table |
| Network scope | Portable caller-owned cells, paths, dipoles, and electrodes | Initially fixed five-cell 1D network and one electrode |
| Spatial scope | Row/column geometry; intended for uniform grids | Initially uniform 1D geometry only |
| Timestep | Supplied to path update at runtime | Compile-time selection with a matching table |
| Numerical error | Native floating-point implementation error | Exhaustively reported table quantization error |
| Execution cost | Geometry-dependent arithmetic each update | Fixed table access and addition |
| WCET analysis | Includes library arithmetic and complex operations | Straight-line bounded integer operations |
| Memory trade-off | Small data footprint | Table memory grows as timestep decreases |

The FlexPRET implementation is therefore not a different physiological model.
It is a target-specific, discretely precomputed realization of the same
moving-dipole equation at the states reachable by the selected timestep.

## 10. Validation requirements

Validation must compare the generated integer result with the original
floating-point `iccnet-core` calculation sample by sample. It must cover:

- inactive paths and zero EGM;
- both propagation directions;
- all path positions;
- simultaneous active paths;
- annihilation;
- path delays from one to five seconds;
- left, centre, right, and dual pacemaker arrangements;
- all five supported timesteps; and
- complete five-cell EGM traces.

Reported metrics will include maximum absolute error, RMSE, peak amplitude
error, peak timing error, sign agreement, memory use, execution time, release
lateness, and HEPTANE WCET once the FlexPRET processor model is validated.
