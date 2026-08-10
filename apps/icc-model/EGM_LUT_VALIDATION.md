# Five-Cell EGM Lookup Generator Validation

Date: 2026-08-09

## Scope

This report validates the host-side lookup-table generator. Runtime validation
is reported separately in `EGM_RUNTIME_VALIDATION.md`.

The reference configuration is:

- five cells from `(0, 0)` through `(24, 0)` mm;
- four adjacent, horizontal, 6 mm paths;
- 1000 ms propagation delay;
- both A-to-B and B-to-A propagation;
- one electrode at grid row 0, column 1, height 1 mm;
- default dipole parameters `(18.0, 1.0, 0.1)`; and
- timesteps of 200, 100, 50, 20, and 10 ms.

## Generated artifacts

`tools/generate_egm_lut.c` produces, for every supported timestep:

- a C header containing the signed 32-bit table;
- a CSV containing every reference and quantized sample; and
- one combined Markdown range and error report.

Reproducible proof artifacts are created under `generated/egm_1d5c/`. The
directory is deliberately ignored by Git; the generator source, its fixed
configuration, and the generation procedure are the source of truth. Across
all timesteps the tables contain 1480 path/direction/progress samples.

## Common integer scale

The generator swept every reachable sample across all five timesteps before
selecting the scale. It reserved conservative accumulator capacity for four
simultaneously active paths and selected:

```text
EGM_LUT_SCALE = 10,000,000 integer units per model potential unit
```

The maximum absolute reference contribution was `7.95309019089`. The resulting
conservative four-path magnitude was `318,123,608`, below the signed 32-bit
limit of `2,147,483,647`.

Across all tables, the maximum absolute conversion error was less than
`5.0e-8` model potential units.

## Independent `iccnet-core` comparison of path 0

The original path-0 generator was compared with the actual EGM implementation from private
repository `therealmcfly/iccnet-core`, commit:

```text
60da0af Use CMake as the native build system
```

An independent host harness constructed real `Icc`, `IccPath`, `PathDipole`,
and `Electrode` objects and called `path_dipole_update()` followed by
`electrode_add_dipole()` for every reachable sample.

The comparison covered:

| Timestep | Samples |
|---:|---:|
| 200 ms | 10 |
| 100 ms | 20 |
| 50 ms | 40 |
| 20 ms | 100 |
| 10 ms | 200 |
| **Total** | **370** |

Results:

```text
core_samples=370
generated_samples=370
mismatches=0
maximum potential difference=0
maximum dipole-position difference=0
```

The printed single-precision dipole positions and potentials therefore matched
`iccnet-core` exactly for path 0. The four-path generator applies the same
validated operation order independently at the translated x coordinates of
paths 1, 2, and 3. This report does not claim a new direct `iccnet-core`
comparison for those three translated paths.

## Deterministic-generation test

The generator was compiled and executed twice. SHA-256 hashes were compared for
all five headers, all five CSV files, and the combined report:

```text
generated files=11
hash differences=0
```

## Reproducible local test

Run:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/generate_egm_luts.sh
./tools/run_egm_lut_tests.sh
```

The script:

1. compiles the host generator with warnings treated as errors;
2. generates all artifacts independently in two temporary directories;
3. checks that the two generations match byte-for-byte;
4. compiles every generated header independently;
5. verifies timestep, cell-count, path-count, delay, step-count, and
   common-scale metadata; and
6. verifies conservative four-path signed 32-bit range.

The separate generation command installs a successful generation into the
application's ignored `generated/` directory and records the generator-source
and output hashes in `GENERATION_SHA256SUMS.txt`.

Expected result:

```text
EGM lookup generator tests passed
```

## Current limitations

- Four paths and one electrode are generated.
- The reference geometry is fixed as constants in the generator source.
- The direct `iccnet-core` comparison currently covers path 0 only.
- Changing path delay, gap, electrode geometry, or EGM parameters requires
  regeneration.
- No physical DE1-SoC or HEPTANE EGM result is claimed yet.
