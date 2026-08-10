# One-Path EGM Lookup Generator Validation

Date: 2026-08-09

## Scope

This report validates the first EGM implementation milestone: the host-side
lookup-table generator. It does not validate an EGM runtime on FlexPRET.

The reference configuration is:

- one horizontal path from `(0, 0)` to `(6, 0)` mm;
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

The committed proof artifacts are under `generated/egm_1path/`.

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

## Independent `iccnet-core` comparison

The generator was compared with the actual EGM implementation from private
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
`iccnet-core` exactly for this reference configuration.

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
./tools/run_egm_lut_tests.sh
```

The script:

1. compiles the host generator with warnings treated as errors;
2. regenerates all artifacts in a temporary directory;
3. checks that regeneration exactly matches the committed artifacts;
4. compiles every generated header independently;
5. verifies timestep, delay, step-count, and common-scale metadata; and
6. verifies conservative four-path signed 32-bit range.

Expected result:

```text
EGM lookup generator tests passed
```

## Current limitations

- Only one path and one electrode are generated.
- The reference geometry is fixed as constants in the generator source.
- The generated tables are not yet selected by the FlexPRET build.
- The FlexPRET path does not yet expose a dedicated progress index.
- No target EGM accumulation code exists.
- No Verilator, DE1-SoC, symbol, memory-map, or HEPTANE EGM result is claimed.

The next milestone is to generalize the generator to all four paths using one
canonical configuration, then add the small integer progress index and the
table-only FlexPRET EGM accumulator.
