# ICC Model on FlexPRET: Step 1

This app contains only a single ICC cell. It deliberately does not yet contain
paths, EGM calculation, controller packets, or GES communication.

The detailed design rationale is in [IMPLEMENTATION.md](IMPLEMENTATION.md).

## Numerical representation

The reference ICC model expresses voltage in millivolts. Before compilation,
each voltage is converted to an integer number of nanovolts:

```text
voltage_nv = voltage_mV × 1,000,000
voltage_mV = voltage_nv / 1,000,000
```

Examples:

```text
-4.494 µV     = -4494 nV
-67.633600 mV = -67633600 nV
```

The nanovolt is the storage unit, so runtime voltage addition and comparison
use ordinary signed integers. All slope-to-increment conversions were
performed before compilation for the fixed 200 ms timestep. The ICC model
update contains no floating-point values, multiplication, division, or
square-root operations. Diagnostic conversion to nearest whole microvolts uses
integer division only for CSV output.

The initial app uses a 20-second pacemaker interval and prints one CSV row per
200 ms model step:

```text
sample,time_ms,state,voltage_nv,nearest_uv
```

## Build and flash on FPGA

FPGA remains the default target. Use a dedicated build directory so its
objects and linker configuration are never mixed with an emulator build.

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cmake -S apps/icc-model -B apps/icc-model/build-fpga -DTARGET=fpga
cmake --build apps/icc-model/build-fpga --target icc-model
```

The build generates:

```text
apps/icc-model/build-fpga/icc-model.mem
apps/icc-model/bin/icc-model
```

Flash it using the same bootloader procedure as the GES app:

```bash
./apps/icc-model/bin/icc-model
```

## Build and run in the emulator

The emulator build uses FlexPRET's Verilator-based `fp-emu`. It must be built
separately because emulator and FPGA applications use different linker and
bootloader configurations.

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cmake -S apps/icc-model -B apps/icc-model/build-emu -DTARGET=emulator
cmake --build apps/icc-model/build-emu --target icc-model
./apps/icc-model/bin/icc-model-emu
```

The final command is equivalent to:

```bash
fp-emu +ispm=apps/icc-model/build-emu/icc-model.mem
```

The ICC model runs continuously. Stop it with `Ctrl+C`.

To check that the emulator itself is available after sourcing `env.bash`:

```bash
fp-emu --hwconfig
```

## Host test

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
gcc -std=c11 -Wall -Wextra -Werror -Iinc \
    src/icc.c tests/test_icc.c -o /tmp/icc-model-test
/tmp/icc-model-test
```

## Target configuration

- `TARGET` accepts `fpga` or `emulator`.
- The default remains `fpga`.
- FPGA and emulator builds use separate build directories.
- `bin/icc-model` is the FPGA launcher.
- `bin/icc-model-emu` is the emulator launcher.
- No path or EGM implementation is included at this stage.
