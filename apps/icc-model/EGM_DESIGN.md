# FlexPRET Relative-Potential EGM Design

## Purpose

The EGM implementation represents a moving dipole on a straight
one-dimensional ICC path. Electrode position is a runtime physical parameter.
One reusable lookup table therefore represents potential as a function of
relative position rather than cell, path, timestep, or absolute coordinate.

## Reduction of the physical equation

The host reference equation projects the vector from dipole to electrode onto
the direction of propagation. In one dimension, let:

```text
d = +1 for A-to-B propagation
d = -1 for B-to-A propagation
r = d * (electrode_x - dipole_x)
```

With fixed electrode height `h`, dipole moment `M`, longitudinal weight `L`,
and transverse weight `T`, the equation becomes:

```text
V(r) = M * (L*r - T*h) / (r*r + h*h)^(3/2)
```

The generator uses `M=18`, `L=1`, `T=0.1`, and `h=1 mm`, preserving the
existing physical equation. The direction factor is essential: it reverses the
longitudinal dipole orientation while leaving the transverse distance
positive. A dipole that has passed the electrode in its propagation direction
has negative `r`, producing the expected negative feature.

The complete potential is not antisymmetric because the transverse term is
negative for both `r` and `-r`. Validation therefore checks the exact pair
identity from the equation rather than incorrectly requiring `V(-r)=-V(r)`.

## Table definition

| Metadata | Value |
|---|---:|
| Minimum oriented relative position | -24,000 um |
| Maximum oriented relative position | +24,000 um |
| Spatial step | 60 um |
| Entry count | 801 |
| Entry type | `int32_t` |
| Logical table storage | 3,204 bytes |
| Electrode height | 1,000 um |
| Integer scale | 10,000,000 |

The ±24 mm range covers every combination of electrode and dipole coordinate
inside the current 0–24 mm network. The endpoints are included:

```text
(24000 - (-24000)) / 60 + 1 = 801
```

The generated header defines exactly one array:

```c
static const int32_t kEgmRelativePotential[801];
```

It also defines range, step, entry-count, height, scale, maximum-entry, and
table-byte metadata. Generated output contains no timestep, path, cell, or
absolute-electrode dimension.

## Integer coordinate lattice

Target code stores coordinates in 60 um units:

```text
Cell 1 =   0 units =     0 um
Cell 2 = 100 units =  6000 um
Cell 3 = 200 units = 12000 um
Cell 4 = 300 units = 18000 um
Cell 5 = 400 units = 24000 um
```

The 6 mm path length is 100 units. For path index `p` and progression `s`:

```text
A-to-B dipole = p*100 + s*stride
B-to-A dipole = (p+1)*100 - s*stride
```

The compile-time stride is 20, 10, 5, 2, or 1 for 200, 100, 50, 20, or
10 ms. Runtime indexing is then:

```text
A-to-B oriented relative = electrode - dipole
B-to-A oriented relative = dipole - electrode
index = oriented_relative + 400
```

There is no runtime unit conversion, division, modulo, interpolation, square
root, or floating point. Bounds checks reject coordinates outside index
`0..800`.

## Runtime electrode state

`IccEgm` contains one signed grid coordinate and an initialization flag. The
API is explicit and has no hidden global state:

```c
bool icc_egm_init(IccEgm *egm, int32_t electrode_x_um);
bool icc_egm_set_electrode_x_um(IccEgm *egm, int32_t electrode_x_um);
bool icc_egm_compute(
    const IccEgm *egm,
    const IccNetwork1d *network,
    IccEgmValue *result);
```

The setter maps exactly `0`, `6000`, `12000`, `18000`, and `24000 um` to the
five grid coordinates using a bounded switch. Other positions are rejected.
Changing the state repeatedly does not modify or regenerate the LUT.

Electrode selection and telemetry selection are independent. The EGM state
changes the spatial coordinate used for potential; a telemetry consumer merely
chooses which already-computed cell voltage to transmit or display. The current
emulator CSV reports all five cells and has no cell filter.

## Fixed bounds and overflow

At most four paths can contribute. The host generator rejects a table if:

```text
4 * maximum_absolute_entry > INT32_MAX
```

The generated maximum is also checked with a target compile-time assertion.
Consequently, accumulation remains signed 32-bit addition with a proven bound;
no 64-bit accumulator or saturation branch is required.

Every target update performs four bounded path checks. An inactive or
annihilated path contributes zero. An active path performs fixed-width integer
coordinate arithmetic, two range checks, one table access, and one addition.
Memory allocation and loop bounds do not depend on input data.

## Configuration validity

The EGM runtime accepts the current geometry only when:

- the network and every path are initialized;
- path `i` connects Cell `i` to Cell `i+1`;
- every gap is 6 mm;
- every delay is 1000 ms;
- the selected electrode is one of the five physical cell coordinates; and
- active progression remains within the compile-time step count.

Failure is returned to the caller. It is never converted silently into a zero
EGM sample. Idle paths legitimately contribute zero.

## Generator and regeneration policy

`tools/generate_egm_lut.c` is a host program and may use double precision and
`sqrt()`. It produces the header, a sample-by-sample CSV, a report, and hashes.
The application requires generation before CMake configuration.

Regeneration is required after changing a physical parameter contained in the
one-dimensional potential function, the integer scale, table range or step,
metadata, or generator algorithm. It is not required for electrode movement,
timestep selection, path activation, propagation direction, or telemetry.

Changing path velocity requires a compatible integer progression mapping. The
potential table remains a function of relative position and is not regenerated
solely because velocity changes.

## WCET properties

The runtime has constant storage, statically bounded iteration, deterministic
indexing, and no data-dependent numerical solver. The EGM object disassembly
contains no floating-point, division, modulo, or square-root instruction and
no arithmetic helper reference. These properties make the function suitable
for a later HEPTANE analysis of the deployed ELF.

Verilator measurements are recorded separately and are not presented as a
formal WCET bound or board measurement.
