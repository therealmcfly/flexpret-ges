# FlexPRET Five-Cell ICC and Relative-Potential EGM Application

This application runs a deterministic five-cell, one-dimensional ICC network
on FlexPRET. Five ICC cells are connected by four bidirectional propagation
paths. EGM contributions are obtained from one signed 32-bit lookup table whose
only independent variable is the oriented relative position between the
electrode and a moving dipole.

Q1, Q2, and Q3 are active propagation states, matching ICCNet Core. Keeping
the preceding cell active during a relay handoff makes the completed path
annihilate instead of launching an artificial reflected dipole.

The target executes integer arithmetic only. Floating point and square root are
confined to the host-side table generator and independent reference test.

## Architecture

For propagation direction `d`, where A-to-B is `+1` and B-to-A is `-1`, the
lookup coordinate is:

```text
oriented_relative_position = d * (electrode_x - dipole_x)
```

The table covers `-24000` through `+24000 um` in `60 um` increments. It has 801
`int32_t` entries and occupies 3,204 bytes. The existing 6 mm path and 1000 ms
delay give a propagation velocity of 6 mm/s, so every supported timestep lands
exactly on the table grid:

| Timestep | Dipole movement | LUT index movement |
|---:|---:|---:|
| 200 ms | 1200 um | 20 |
| 100 ms | 600 um | 10 |
| 50 ms | 300 um | 5 |
| 20 ms | 120 um | 2 |
| 10 ms | 60 um | 1 |

The electrode may be moved at runtime among the five cell positions: `0`,
`6000`, `12000`, `18000`, and `24000 um`. Moving it does not regenerate the
table and does not rebuild the application.

The build uses separate entry points for separate purposes. `src/main.c` is
the minimal FPGA scheduler, `src/emulator_main.c` provides continuous CSV
telemetry, and `src/verilator_test_main.c` contains finite validation scenarios.
All three use the shared initialization and model-step API in `src/app.c`.
Test scenarios are therefore absent from the FPGA production translation unit.

## Environment setup

The verified environment is WSL Ubuntu with the FlexPRET repository at:

```text
/home/eugene/gastric-pacemaker/fp-ges
```

Load the FlexPRET environment and RISC-V compiler before configuring a target:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
```

Confirm the tools:

```bash
fp-emu --hwconfig
riscv-none-elf-gcc --version
cmake --version
gcc --version
```

## Generate the lookup table

Generated files are deliberately ignored by Git. Run the generator after a
clone and before CMake configuration:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/generate_egm_luts.sh
```

This creates:

```text
generated/egm_relative/egm_relative_lut.h
generated/egm_relative/egm_relative_lut.csv
generated/egm_relative/EGM_LUT_GENERATION_REPORT.md
generated/egm_relative/GENERATION_SHA256SUMS.txt
```

Regenerate after changing the EGM equation, electrode height, dipole moment,
longitudinal or transverse weight, integer scale, relative-position range,
spatial resolution, generator implementation, or generated metadata.

Do not regenerate merely because the electrode moves, the simulation timestep
changes, a different path becomes active, propagation reverses, or another cell
is examined in telemetry.

The present runtime accepts only uniform 6 mm paths with 1000 ms delay. A
different gap or delay is rejected because the division-free coordinate
mapping is calibrated to 6 mm/s. Supporting another velocity requires a new
integer coordinate mapping, but it does not alter the physical potential as a
function of relative position.

## Host tests

Run all host checks:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/generate_egm_luts.sh
./tools/run_icc_host_tests.sh
./tools/run_egm_lut_tests.sh
./tools/run_egm_runtime_tests.sh
```

The validation performed on 2026-08-16 passed:

- ICC, path, and network tests at 200, 100, 50, 20, and 10 ms;
- two independent, byte-identical table generations;
- all 801 entries against an independent double-precision equation;
- both table endpoints, zero offset, near-electrode signs, and the transverse
  symmetry identity;
- five electrode positions, four paths, two directions, and every legal path
  progression step at every timestep;
- first and last active progression states;
- repeated electrode changes in one process;
- invalid electrodes, gaps, delays, topology, progression, and null inputs;
- Q1/negative-EGM alignment for every cell and timestep.

The measured maximum absolute table error was
`4.999771405223008e-08` potential units at `21780 um`. The maximum meaningful
relative error was `1.4873562175373967e-06` at `-23640 um`, using a
`1e-5`-unit reference-magnitude threshold. The absolute acceptance bound was
`5.00001e-8`, slightly above half of one `1e-7` output unit.

## Verilator tests

Run the finite automatically terminating matrix:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/run_egm_verilator_tests.sh
```

The script runs all five electrode positions at all five timesteps. Cells 1-4
are checked with A-to-B propagation; Cell 5 is checked with B-to-A propagation.
Both scenarios propagate through all four paths. Functional runs print every
EGM sample, while separate no-trace runs measure execution and scheduling.

The 2026-08-16 matrix contained 25 passing configurations. Each terminated at
or after 4500 ms of biological time. Every Q1 activation occurred at the
expected 1000 ms path spacing. At the accelerated 1,000,000 ns release period:

- worst measured ICC/path/EGM execution time: `25,420 ns`;
- maximum release lateness: `160 ns`;
- 10 ms biological deadline margin: `9,974,640 ns` in the worst 10 ms case.

These are cycle-accurate Verilator observations, not a formal WCET proof and
not DE1-SoC board measurements. Results and representative traces are in
`validation/egm_relative/`.

## All-cell waveform records

Run the natural-pacemaker waveform matrix with:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
./tools/run_egm_waveform_tests.sh
```

This performs 25 additional finite Verilator runs: all five electrode
positions at all five timesteps. Scenario 0 gives Cells 1-5 intrinsic intervals
of 20, 23, 26, 30, and 40 s. No cell state is forced. The initial simultaneous
WAIT-state release is retained in the raw trace, while the recorded waveform
window uses the next natural Cell 1 Q1 and includes one second of pre-Q1
baseline. The script asserts an intrinsic Cell 1 event, path-driven activation
of Cells 2-5 at 1000 ms spacing, the selected electrode, and automatic
termination. It creates combined waveform CSVs, Q1 event CSVs, and SVG figures
under `generated/egm_relative/waveforms_natural_a_to_b/`.

The tracked numerical records and waveform figures are under
`validation/egm_relative/waveforms/`. Their hashes, extrema, column format,
boundary interpretation, and reproduction details are recorded in
`EGM_ALL_TIMESTEP_WAVEFORMS.md`.

## Build for the FPGA

Running ICC-model on the DE1-SoC has three separate stages:

1. Build and program the FlexPRET bootloader FPGA image.
2. Compile ICC-model into a FlexPRET application image.
3. Send the application image to the running bootloader over UART0.

### 1. Build and program the FlexPRET bootloader

From VS Code, run:

```text
Ctrl+Shift+P
Tasks: Run Task
Mega: Build and Synthesize Bootloader FPGA
```

The task builds the bootloader SDK, generates the DE1-SoC FlexPRET hardware,
runs Quartus synthesis, and opens Quartus Programmer. In Quartus Programmer:

1. Select the DE1-SoC USB-Blaster hardware (`DE-SoC [USB-1]`) and JTAG mode.
2. Add the generated image:

   ```text
   Z:/home/eugene/gastric-pacemaker/fp-ges/build/fpga/de1-soc/fp-bootloader/de1soc_bootloader.sof
   ```

3. Check `Program/Configure` for the `5CSEMA5F31` device.
4. Click `Start`.

The full synthesis task is only required after changing FlexPRET or bootloader
source. After a power cycle, the existing `.sof` can be programmed by opening
Quartus Programmer directly:

```bash
/mnt/c/intelFPGA_lite/18.1/quartus/bin64/quartus_pgmw.exe
```

Pressing `KEY[3]` resets FlexPRET but does not erase the programmed FPGA image,
so a reset alone does not require another synthesis or Quartus programming run.

### 2. Compile ICC-model

For a normal build using all default settings:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cd apps/icc-model
./tools/generate_egm_luts.sh
cmake -S . -B build
cmake --build build
```

`cmake -S . -B build` reads `CMakeLists.txt`, validates the configuration,
stores the selected values in `build/CMakeCache.txt`, and generates the build
files. It does not compile or flash the application.

`cmake --build build` compiles the default targets. This project has one main
application target, `icc-model`, so `--target icc-model` is optional. The build
also produces the `.mem` image required by the FlexPRET bootloader.

The build creates `build/icc-model.mem` and the generated flash launcher
`bin/icc-model`.

### 3. Load ICC-model through the bootloader

Connect a 3.3 V USB-UART adapter to FlexPRET UART0. TX and RX must be crossed;
the wiring and USB attachment details are in
[Hardware programming and application flashing](#hardware-programming-and-application-flashing).

Before loading the application:

1. Set `SW[0]` to `ON` / `1` to select dynamic UART loading.
2. Press `KEY[3]` to reset FlexPRET into the bootloader.
3. Keep `SW[0]` high while running:

   ```bash
   ./bin/icc-model
   ```

The launcher serializes `build/icc-model.mem`, transfers it over UART0 at
115200 baud, and opens `picocom`. Exit with `Ctrl+A`, followed by `Ctrl+X`.

The FPGA application configuration defaults are:

- Cell 1 interval: `20` seconds;
- Cell 2-5 intervals: `0` seconds;
- all four path delays: `1000` ms;
- all four path gaps: `6` mm;
- EGM and pacing UART: UART2;
- EGM electrode and pacing lead: `6000` micrometres (Cell 2);
- timestep: `200` ms.

After initialization, ICC-model prints the effective configuration once on
UART0. The binary EGM/pacing UART remains free of diagnostic text. With the
defaults, the console begins with:

```text
--------------- ICC Model on FlexPRET Start ---------------
Timestep:             200 ms
Cell intervals:       [20, 0, 0, 0, 0] s
Path delays:          [1000, 1000, 1000, 1000] ms
Path gaps:            [6, 6, 6, 6] mm
Electrode position:   6000 um
Electrode cell:       Cell 2
Pacing-lead cell:     Cell 2
EGM/pacing UART:      UART2
EGM frame:            AA 55 + little-endian int16
ICC Model Start!
```

Cell intervals, path geometry, and UART can be set when configuring:

```bash
cmake -S . -B build-fpga-50ms \
  -DTARGET=fpga \
  -DICC_MODEL_TIMESTEP_MS=50 \
  -DICC_EGM_ELECTRODE_X_UM=0 \
  -DICC_CELL1_INTERVAL_S=20 \
  -DICC_CELL2_INTERVAL_S=0 \
  -DICC_CELL3_INTERVAL_S=0 \
  -DICC_CELL4_INTERVAL_S=0 \
  -DICC_CELL5_INTERVAL_S=0 \
  -DICC_PATH1_DELAY_MS=1000 \
  -DICC_PATH2_DELAY_MS=1000 \
  -DICC_PATH3_DELAY_MS=1000 \
  -DICC_PATH4_DELAY_MS=1000 \
  -DICC_PATH1_GAP_MM=6 \
  -DICC_PATH2_GAP_MM=6 \
  -DICC_PATH3_GAP_MM=6 \
  -DICC_PATH4_GAP_MM=6 \
  -DICC_MODEL_UART=2
cmake --build build-fpga-50ms
```

Supported cell intervals are `-1, 0, 15, 20, 23, 26, 30, 40` seconds.
Use `0` for no intrinsic activation while retaining pacing response. A cell
configured as `-1` ignores pacing as well as intrinsic activation. UART1 and
UART2 are supported; UART0 remains reserved for the bootloader and console.

The current EGM LUT specialization requires every delay to remain `1000` ms
and every gap to remain `6` mm. CMake rejects other path values rather than
building an application with inconsistent EGM geometry. Baud rate and the
`AA 55` plus little-endian `int16` packet format are intentionally fixed.

CMake stores these values in each build directory. Reconfiguring without a
`-D` option retains that directory's previous value; `cmake --build` uses
the cached configuration and does not restore defaults.

The initial electrode value is a startup setting only. Target code can call
`icc_egm_set_electrode_x_um()` later without rebuilding or regenerating.

To cross-build and inspect all supported timesteps:

```bash
./tools/run_egm_fpga_checks.sh
```

All five FPGA builds passed after the entry-point refactor on 2026-08-18. The
largest build was the 100 ms
configuration:

| Quantity | Bytes |
|---|---:|
| ISPM used | 16,036 |
| DSPM static used | 6,272 |
| Reserved stack | 2,048 |
| Total SPM used or reserved | 24,356 |
| Combined configured ISPM + DSPM | 131,072 |
| Remaining SPM | 106,716 |

The linker places the 3,204-byte constant table in `.data`, so its load image
is present in ISPM and its runtime copy is present in DSPM. The table symbol
itself remains exactly 3,204 bytes.

## Runtime electrode API

The public interface in `inc/egm.h` is:

```c
bool icc_egm_init(IccEgm *egm, int32_t electrode_x_um);
bool icc_egm_set_electrode_x_um(IccEgm *egm, int32_t electrode_x_um);
int32_t icc_egm_electrode_x_um(const IccEgm *egm);
bool icc_egm_compute(
    const IccEgm *egm,
    const IccNetwork1d *network,
    IccEgmValue *result);
```

The setter accepts only the five physical cell coordinates. Invalid values are
rejected and do not replace the current electrode. EGM state is passed
explicitly; there is no hidden global electrode.

The current emulator CSV reports every cell. It does not have a telemetry-cell
filter. A downstream telemetry choice determines which ICC voltage is sent or
displayed; it must not modify `IccEgm.electrode_position_units`. Conversely,
moving the EGM electrode changes only the EGM coordinate and does not select an
ICC telemetry channel.

The FPGA application currently computes one EGM electrode per execution. A
future, optional five-channel design is recorded in
`MULTI_ELECTRODE_FUTURE_DESIGN.md`. It preserves the single 801-entry LUT and
the current single-electrode mode, but has not been implemented or validated.

## CSV columns

The generated `egm_relative_lut.csv` contains:

| Column | Meaning |
|---|---|
| `oriented_relative_position_um` | Signed LUT coordinate in micrometres. |
| `reference_potential` | Host double-precision equation result. |
| `scaled_integer` | Stored signed value at scale 10,000,000. |
| `reconstructed_potential` | Stored integer divided by the scale. |
| `quantization_error` | Reconstructed minus reference value. |
| `absolute_error` | Absolute quantisation error. |

Ordinary emulator output contains sample and biological time, FlexPRET time,
measured period, release lateness, measured execution time, electrode x,
states and integer-nanovolt voltages for all five cells, the scaled EGM, and
all four path states.

Its exact header is:

```text
sample,time_ms,fpga_time_ns,period_ns,release_lateness_ns,execution_time_ns,egm_electrode_x_um,cell_0_state,cell_0_nv,cell_1_state,cell_1_nv,cell_2_state,cell_2_nv,cell_3_state,cell_3_nv,cell_4_state,cell_4_nv,egm_scaled,path_0_state,path_1_state,path_2_state,path_3_state
```

## Hardware programming and application flashing

The original 2026-08-15 software validation did not flash a board. Subsequent
physical UART2 pacing tests flashed the FPGA application and verified repeated
pacing responses on the DE1-SoC.

To prepare the DE1-SoC manually:

1. Connect power, USB-Blaster, and a 3.3 V USB-UART adapter.
2. Program the FlexPRET bootloader `.sof` using the procedure above.
3. Wire the adapter to FlexPRET UART0 with TX and RX crossed:

   ```text
   USB-UART TX  -> GPIO_0[1]  FlexPRET UART0 RX
   USB-UART RX  -> GPIO_0[0]  FlexPRET UART0 TX
   USB-UART GND -> GPIO_0[2]  GND
   ```

   Do not connect a 5 V UART signal.

4. From Windows PowerShell, find the USB-UART adapter:

   ```powershell
   usbipd list
   ```

5. If it is not already shared, bind it from an Administrator PowerShell:

   ```powershell
   usbipd bind --busid <BUSID>
   ```

6. Attach it to WSL:

   ```powershell
   usbipd attach --wsl --busid <BUSID>
   ```

7. In WSL, confirm the device and grant temporary access if required:

   ```bash
   ls -l /dev/ttyUSB*
   sudo chmod 666 /dev/ttyUSB0
   ```

   The permanent alternative is to add the user to the `dialout` group and
   sign out and back in. `chmod +x` is not a serial-port permission fix.

8. Set `SW[0]` high and press `KEY[3]` to enter dynamic loading mode.
9. Build the FPGA application as shown above, then load it:

   ```bash
   cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
   ./bin/icc-model
   ```

The default bootloader UART is `/dev/ttyUSB0`. To use another device, set the
environment variable before running CMake because the path is written into the
generated launcher:

```bash
export FP_SDK_FPGA_FLASH_DEVICE=/dev/ttyUSB1
cmake -S . -B build
cmake --build build
./bin/icc-model
```

Only one program should own `/dev/ttyUSB0`. Close `picocom`, `minicom`, or any
other serial terminal before running the launcher.

## Runtime instruction restrictions

The RISC-V EGM object was inspected at every timestep. It contains no
floating-point, division, modulo, or square-root instruction and references no
floating-point, square-root, division/modulo, or 64-bit arithmetic helper.

The complete ELF contains `__divsi3`, `__udivsi3`, `__modsi3`, and
`__umodsi3`. They are outside the EGM implementation: network initialization
checks runtime path-delay divisibility and the linked integer-printing library
formats output. No 64-bit arithmetic helper, floating conversion helper, or
square-root helper was present.

## Limitations

- Only a straight five-cell 1D geometry is implemented.
- All electrodes share the fixed 1 mm height used to generate the table.
- Runtime electrode positions are restricted to the five cell centres.
- EGM runtime geometry currently requires 6 mm gaps and 1000 ms path delays.
- Table values are model-potential units, not calibrated clinical millivolts.
- Verilator timing is evidence from simulation, not formal WCET analysis.
- HEPTANE analysis and DE1-SoC timing/measurement remain to be performed.
- No FPGA was flashed as part of this work.
