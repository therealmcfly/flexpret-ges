# Relative-Potential EGM LUT Validation

Validation date: 2026-08-15

## Commands

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/generate_egm_luts.sh
./tools/run_egm_lut_tests.sh
cd generated/egm_relative
sha256sum -c GENERATION_SHA256SUMS.txt
```

All commands completed successfully.

## Reproducibility

The test compiled the host generator once and ran it into two independent
temporary directories. A recursive byte comparison found no difference.

| Result | Value |
|---|---|
| Entry count | 801 |
| Logical table bytes | 3,204 |
| Generated header file bytes | 11,082 |
| First header SHA-256 | `5f35a1066bcd0b8a745c1545eaeb1fd03b233a0b4c0892d18eeb7cca3f2623bd` |
| Second header SHA-256 | `5f35a1066bcd0b8a745c1545eaeb1fd03b233a0b4c0892d18eeb7cca3f2623bd` |
| Byte comparison | Pass |

The installed generation hashes were:

```text
9f1577dbf5945b1f6da70c74eb61deba96d77b53584922e0861a3e78ff34571e  tools/generate_egm_lut.c
6f1939bd7ad44ca5b965d243298749b97beff34dc746e3d7682d30dfa773e606  EGM_LUT_GENERATION_REPORT.md
0e4d5ff60babe1f3cfebe016b90da7f20085031517b6fcaa9c9d72d62a7ff251  egm_relative_lut.csv
5f35a1066bcd0b8a745c1545eaeb1fd03b233a0b4c0892d18eeb7cca3f2623bd  egm_relative_lut.h
```

## Independent numerical reference

`tests/test_egm_lut.c` independently implements the physical equation in
double precision. It does not call generator functions or use production
lookup/indexing code as its reference. Every one of the 801 stored integers was
reconstructed and compared with that equation.

The integer scale is 10,000,000, giving one stored unit equal to `1e-7` model
potential units. Rounding to nearest limits ideal quantisation to half a stored
unit, or `5e-8`. The pass bound was `5.00001e-8`, allowing only a very small
binary-double comparison allowance above that theoretical limit.

| Measurement | Result |
|---|---:|
| Maximum absolute error | `4.999771405223008e-08` |
| Worst absolute-error position | `21780 um` |
| Maximum meaningful relative error | `1.4873562175373967e-06` |
| Worst meaningful-relative position | `-23640 um` |
| Meaningful reference threshold | `1e-5` |
| Absolute tolerance | `5.00001e-8` |
| Result | Pass |

Relative error is not meaningful arbitrarily close to a zero crossing. It is
therefore reported only where the independent reference magnitude is at least
100 integer least-significant units (`1e-5` potential units). Absolute error is
the primary acceptance measure and covers all entries without exclusion.

## Boundary and sign checks

Automated assertions covered:

- index 0 at `-24000 um`;
- index 800 at `+24000 um`;
- the exact zero-offset value of `-1.8`, stored as `-18000000`;
- negative potential at `-600 um`;
- positive potential at `+600 um`;
- a stronger negative feature at `-600 um` than at zero;
- all positive/negative position pairs using the correct transverse-term
  identity.

The full table is not antisymmetric. Its longitudinal component is
antisymmetric, while its negative transverse component is symmetric. The test
uses the physical equation for the sum of each `+r/-r` pair.

## Range and accumulation

The generator measures the largest absolute stored entry and refuses output if
four simultaneous contributions could exceed `INT32_MAX`. The generated
metadata is checked again by a target compile-time assertion. This proves that
the four-path accumulator can remain signed 32-bit without overflow.

## Evidence

Machine-readable output is stored in:

```text
validation/egm_relative/lut_validation_output.txt
validation/egm_relative/generation_sha256sums.txt
```

The generated header and CSV are intentionally excluded from Git and are
recreated with `./tools/generate_egm_luts.sh`.
