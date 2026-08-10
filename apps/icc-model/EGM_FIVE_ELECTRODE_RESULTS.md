# Five-Electrode EGM Waveform Results

Date: 2026-08-10

## Purpose

This report presents EGM waveforms for five virtual electrodes positioned
above the five cells in the one-dimensional ICC network. It compares every
biological timestep supported by the FlexPRET implementation: 200, 100, 50,
20, and 10 ms.

The current FlexPRET runtime calculates the electrode above cell 2. The
five-electrode dataset is a host-generated extension used to examine the
spatial waveform pattern and to define the data required for a future
multi-electrode target implementation.

## Reference configuration

- Five cells at x coordinates 0, 6, 12, 18, and 24 mm.
- Four adjacent bidirectional paths, each 6 mm long.
- A 1000 ms propagation delay on every path.
- Five electrodes, each 1 mm above one cell.
- One activation wave travelling from cell 1 to cell 5.
- A-to-B propagation through paths 1, 2, 3, and 4 in sequence.
- Dipole parameters: moment 18, longitudinal weight 1, transverse weight 0.1.
- Integer scale: 10,000,000 units per model potential unit.

The propagation occupies four seconds. At each path boundary, the old path is
inactive and the next path begins at progress zero, matching the reachable
state ordering in the FlexPRET path model.

## Generated datasets

The detailed dataset is stored in
`generated/egm_1d5c/egm_five_electrode_waveforms.csv`. It contains 3700 data
rows: every combination of supported timestep, five electrodes, and every
reachable propagation sample. The directory is reproducibly generated and is
not tracked by Git.

The compact statistical dataset is stored in
`generated/egm_1d5c/egm_five_electrode_summary.csv`. It contains 25 rows, one
for each timestep and electrode combination.

| Timestep | Samples per electrode | Total samples |
|---:|---:|---:|
| 200 ms | 20 | 100 |
| 100 ms | 40 | 200 |
| 50 ms | 80 | 400 |
| 20 ms | 200 | 1000 |
| 10 ms | 400 | 2000 |
| **Total** | **740** | **3700** |

## Peak absolute EGM results

Values are expressed in the model's potential units.

| Timestep | Electrode 1 | Electrode 2 | Electrode 3 | Electrode 4 | Electrode 5 |
|---:|---:|---:|---:|---:|---:|
| 200 ms | 6.139473 | 6.139474 | 6.139474 | 6.139470 | 5.194936 |
| 100 ms | 7.944420 | 7.944419 | 7.944419 | 7.944419 | 5.674586 |
| 50 ms | 7.944420 | 7.944419 | 7.944419 | 7.944419 | 5.913502 |
| 20 ms | 7.944420 | 7.944419 | 7.944419 | 7.944419 | 5.979825 |
| 10 ms | 7.953091 | 7.953090 | 7.953090 | 7.953090 | 6.000540 |

## Interpretation

Each electrode records the same propagating dipole from a different spatial
position. The largest deflection shifts later in time as the activation wave
moves from cell 1 toward cell 5. Electrodes near the active path show the
largest and sharpest biphasic response; more distant electrodes show smaller,
smoother contributions.

Reducing the timestep does not change the underlying moving-dipole equation.
It samples that equation more densely. Coarse timesteps can miss a narrow peak,
which explains why the 200 ms peak magnitude is lower than the 10 ms result.
This is temporal discretization, not fixed-point quantization error.

The final path endpoint is deliberately excluded. For a path delay D and
timestep dt, the reachable samples are 0 through D - dt. This matches the
FlexPRET path state machine and prevents the dataset from containing positions
that the target runtime cannot observe.

## Detailed CSV columns

| Column | Meaning |
|---|---|
| `timestep_ms` | Biological model timestep in milliseconds. |
| `electrode_cell` | One-based cell number beneath the electrode. |
| `electrode_col` | Zero-based electrode/network column. |
| `electrode_x_mm` | Electrode x coordinate in millimetres. |
| `time_ms` | Time since the start of the four-path propagation. |
| `path_index` | Zero-based active path index. |
| `path_cell_a` | One-based source-side cell number. |
| `path_cell_b` | One-based destination-side cell number. |
| `path_elapsed_ms` | Elapsed propagation time on the active path. |
| `direction` | Propagation direction for this experiment. |
| `reference_potential` | Single-precision host reference EGM. |
| `scaled_integer` | Signed integer stored at scale 10,000,000. |
| `reconstructed_potential` | Integer value divided by the scale. |
| `quantization_error` | Reconstructed value minus reference value. |

## Reproduction

Run:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/generate_egm_luts.sh
./tools/run_egm_lut_tests.sh
```

The generation command recreates the lookup tables, datasets, report, and
`GENERATION_SHA256SUMS.txt`. The test then generates two independent temporary
copies, compares them byte-for-byte, verifies the expected 3700 detailed rows
and 25 summary rows, and rejects duplicate timestep/electrode/time keys. For a
published experiment, archive the generated directory or exported result files
alongside the checksum manifest rather than relying on the Git working tree.

## Limitations

- These are model potential units, not calibrated millivolts.
- The five-electrode result is generated on the host; only electrode 2 is
  currently linked into the FlexPRET runtime.
- The experiment contains one left-to-right propagation wave.
- Physical FPGA acquisition, noise, filtering, electrode impedance, and tissue
  volume-conductor validation are outside the present scope.
