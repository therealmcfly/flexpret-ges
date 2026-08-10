# FlexPRET EGM Runtime Validation

Date: 2026-08-10

## Implemented scope

The first target-side EGM model is integrated into the autonomous five-cell
one-dimensional application. It supports:

- five ICCs and four adjacent bidirectional paths;
- one electrode at row 0, column 1, height 1 mm;
- 6 mm gaps and 1000 ms path delays;
- 200, 100, 50, 20, and 10 ms compile-time timesteps; and
- one signed 32-bit EGM result per model step.

The target does not evaluate the moving-dipole equation. Floating-point
geometry is evaluated by `tools/generate_egm_lut.c` on the host. FlexPRET uses
the path state and `progress_step` to select four signed 32-bit contributions
and add them.

## Runtime ordering

Each periodic iteration performs:

```text
update five ICCs
update four paths
select one contribution for each active path
add the four contributions
store or report the EGM sample
```

Propagation start uses progress step 0. Every subsequent path update increments
the index once. The relay state retains the last reachable table entry. Idle
and annihilated paths contribute zero.

## Host integration tests

Run:

```bash
./tools/generate_egm_luts.sh
./tools/run_egm_lut_tests.sh
./tools/run_egm_runtime_tests.sh
```

The runtime test was compiled separately for 200, 100, 50, 20, and 10 ms. At
each timestep it checked metadata selection, inactive paths, both directions,
multiple-path summation, annihilation, an out-of-range guard, configuration
mismatch detection, and every progress step produced by the real path state
machine. All five configurations passed.

## Verilator result

A 200 ms FlexPRET emulator build ran scenario 0 for 150 samples with EGM trace
enabled. The output remained zero while no path was propagating. Cell 0 began
an intrinsic activation at 25.2 seconds, producing the following start of the
propagating EGM trace:

```text
Q1,126,25200,0,intrinsic
EGM,126,25200,4718695
EGM,127,25400,7177425
EGM,128,25600,12078484
EGM,129,25800,23554850
EGM,130,26000,51949401
Q1,131,26200,1,path
EGM,131,26200,-18000001
```

Subsequent path-triggered activations occurred at 27.2, 28.2, and 29.2 seconds
for cells 2, 3, and 4. This demonstrates the complete chain from ICC activation
through path propagation, lookup selection, and EGM output in Verilator.

## FlexPRET memory results

| Build | ROM | RAM | ROM region | RAM region |
|---:|---:|---:|---:|---:|
| 200 ms FPGA | 12,032 B | 3,232 B | 64 KB | 64 KB |
| 200 ms emulator with trace | 13,408 B | 3,456 B | 64 KB | 64 KB |
| 10 ms emulator with trace | 16,544 B | 6,496 B | 64 KB | 64 KB |

The 10 ms configuration is the largest supported table and used 25.24% of ROM
and 9.91% of RAM.

## Target instruction check

The RISC-V disassembly of `icc_egm_1d5c_compute()` contains four bounded helper
calls and three 32-bit `add` instructions. It contains no floating-point,
division, square-root, multiplication, or 64-bit arithmetic instruction. The
linked binary has no unresolved compiler arithmetic helper; `_vectors` is the
only undefined symbol reported by `nm`, as expected for the platform image.

## Remaining validation

- Run the same trace on the physical DE1-SoC.
- Compare complete four-path traces directly with `iccnet-core`.
- Establish WCET with the reviewed FlexPRET HEPTANE configuration.
- Generalize generation when path delays, gaps, or electrode geometry become
  configurable.
