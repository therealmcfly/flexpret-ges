# FlexPRET HEPTANE Audit Record

Date: 2026-08-09

## Purpose

This record captures the initial review of the FlexPRET architecture extension
used with HEPTANE in the previous gastric pacemaker project. It is intended to
guide the later WCET setup for the FlexPRET ICC and EGM application.

## Repositories inspected

- `/home/eugene/gastric-pacemaker/heptane-fp`
- `/home/eugene/gastric-pacemaker/heptane-fp-o0`
- `/home/eugene/gastric-pacemaker/heptane-fp-0107`
- `/home/eugene/gastric-pacemaker/fp-ges`

The primary HEPTANE repository is `heptane-fp` on branch `main`. The other two
directories are detached experimental worktrees. The primary checkout contains
uncommitted edits to the FlexPRET extraction configuration and instruction
decoder; these edits were not changed during the audit.

## Positive findings

- FLEXPRET is registered as a distinct HEPTANE target.
- The target is little-endian and based on RISC-V register and address
  semantics.
- A FlexPRET-specific pipeline-analysis class is selected by the HEPTANE
  configuration.
- The pipeline depth is set to five stages, consistent with FlexPRET.
- Instruction and data scratchpad memories are represented using perfect-cache
  abstractions rather than variable-latency caches.
- Loop-bound annotations are present in the previous pacemaker benchmark.
- The extraction, pipeline analysis, and IPET stages execute and produce a
  numerical result.

## Findings requiring validation or correction

### 1. The analyzed program is not the deployed FlexPRET binary

The previous HEPTANE workflow recompiles an isolated `pm.c` using the bundled
GCC 9.2.0 toolchain. The current FlexPRET SDK uses a different compiler and
different flags. Consequently, the instructions analyzed by HEPTANE are not
guaranteed to be the instructions flashed to the FPGA.

For the ICC/EGM project, HEPTANE must either consume the actual deployed binary
or reproduce the exact compiler version, optimization flags, ISA flags, linker
configuration, SDK code, and compiler helper functions.

### 2. The pipeline model is adapted from the HEPTANE MIPS model

`FLEXPRETPipelineAnalysis` inherits `MIPSPipelineAnalysis` and overrides the
instruction scheduling functions to represent IF, ID, EX, MEM, and WB stages.
This is a useful starting point, but the dependency, bypass, functional-unit,
load-use, branch-flush, and write-back behavior has not yet been demonstrated
to match the selected FlexPRET RTL configuration.

### 3. FlexPRET hardware-thread scheduling is not represented

The current FPGA configuration builds four hardware threads with the flexible
scheduler. The HEPTANE pipeline model behaves like a continuously issued
single-thread pipeline. This can only be used under an explicit and verified
execution assumption, such as thread 0 being the only active thread and being
eligible every cycle. Otherwise scheduler slots and competing threads must be
included in the timing model.

### 4. Instruction timing values have no retained validation evidence

`fp_inst_timing.csv` assigns one cycle to most ALU operations, two cycles to
loads, three cycles to branches and jumps, and five cycles to `du` and `wu`.
No retained microbenchmark, RTL derivation, or target-measurement report was
found that proves these values safely upper-bound the configured processor.

The latency file uses a default value of 33 cycles for an unrecognized
instruction. Unknown instructions should instead stop the analysis because an
arbitrary fallback cannot establish a sound bound.

### 5. Scratchpad timing requires explicit accounting

The WCET configuration uses a one-cycle perfect instruction cache, a zero-cycle
perfect data cache, and zero additional memory latency. Loads separately have a
two-cycle instruction latency. This may represent the pipeline correctly, but
it must be checked for omitted or double-counted latency. Memory-mapped I/O must
not be treated as ordinary zero-latency DSPM access.

### 6. Unsupported ISA entries should be removed or rejected

The architecture decoder contains RV64, multiplication/division-extension, and
floating-point instructions that the configured RV32I FlexPRET does not execute
natively. A project-specific model should accept only instructions that can
appear in the deployed binary and should fail on anything else.

### 7. Saved optimization results are inconsistently labelled

The `reconstruct_o0` and `reconstruct_o1` results stored in the primary
`heptane-fp` checkout contain identical binaries and both specify `-O1`; both
report 1468 cycles. The separate `heptane-fp-o0` worktree contains an actual
`-O0` result of 4270 cycles. The `heptane-fp-0107` result belongs to an earlier
source version and reports 2115 cycles.

Future result directories must record the source commit, compiler version,
compiler flags, FlexPRET hardware configuration, HEPTANE model commit, entry
point, and annotations.

### 8. End-to-end soundness has not been demonstrated

No retained comparison was found between HEPTANE predictions and cycle
measurements from Verilator or the DE1-SoC for the same binary and execution
conditions. The existing result should therefore be described as a prototype
estimate, not yet as a validated WCET bound.

## Required validation plan

1. Freeze the exact FlexPRET hardware configuration, clock, thread modes, and
   scheduler slots.
2. Freeze the application and SDK commits plus compiler version and flags.
3. Make unknown or unsupported instructions fatal during extraction.
4. Create target microbenchmarks for independent and dependent ALU operations,
   shifts, loads, load-use hazards, stores, taken and untaken branches, calls,
   returns, timing instructions, and any compiler helper routines.
5. Measure the same binaries in Verilator and on the DE1-SoC.
6. Compare measured basic-block timing with the HEPTANE pipeline prediction and
   correct the processor model where necessary.
7. Validate complete ICC cell, path, network, and EGM entry points.
8. Archive every configuration and result with immutable commit identifiers.

## Consequence for the EGM design

The target EGM should use fixed-size data, fixed control flow, 32-bit integer
addition and shifts, compile-time geometry, and no floating point, division,
square root, dynamic allocation, or compiler arithmetic helpers. This reduces
both execution cost and the effort required to establish a trustworthy WCET
model.
