# ICC Model on FlexPRET FPGA

This app runs a one-dimensional network of five ICC cells connected by four
integer-timed bidirectional paths on FlexPRET on the DE1-SoC FPGA. The five
cell intervals and four path delays and gaps are compiled into the application.
The FlexPRET bootloader is programmed onto the FPGA first, then the app is sent
to instruction scratchpad memory over UART.

This stage includes the ICC, path, and 1D network models but deliberately does
not yet contain controller integration, EGM calculation, or GES communication.
Design details are in [IMPLEMENTATION.md](IMPLEMENTATION.md). The 40-case
pacemaker-location and path-delay campaign is reported in
[VERILATOR_TEST_RESULTS.md](VERILATOR_TEST_RESULTS.md). Timestep sensitivity
from 200 ms down to 10 ms is reported in
[TIMESTEP_TEST_RESULTS.md](TIMESTEP_TEST_RESULTS.md).

## How FPGA Loading Works

There are two separate things to load:

1. **FlexPRET bootloader FPGA image** — programmed through the DE1-SoC
   USB-Blaster in Quartus Programmer.
2. **ICC application** — built as `icc-model.mem` and sent to the running
   bootloader through a 3.3 V USB-UART adapter.

Rebuild and resynthesize the bootloader only after changing FPGA or bootloader
source. If the board was merely power-cycled, program the existing `.sof`
again. Rebuilding the ICC application does not require FPGA synthesis.

## 1. Build and Program the Bootloader FPGA Image

If the same bootloader-enabled FlexPRET image used for GES is already
programmed and the board has not been power-cycled, continue to
[2. Build the ICC Application](#2-build-the-icc-application).

For a full bootloader build, open the repository in VS Code and run:

```text
Ctrl+Shift+P
Tasks: Run Task
Mega: Build and Synthesize Bootloader FPGA
```

The task:

1. Builds the FlexPRET bootloader SDK.
2. Generates the DE1-SoC FlexPRET Verilog.
3. Runs the Quartus bootloader project flow.
4. Opens Quartus Programmer.

When Quartus Programmer opens:

1. Connect the DE1-SoC USB-Blaster cable.
2. Click `Hardware Setup...` and select `DE-SoC [USB-1]` if necessary.
3. Keep `Mode` set to `JTAG`.
4. Click `Add File...` and select:

```text
build/fpga/de1-soc/fp-bootloader/de1soc_bootloader.sof
```

In the Windows file picker, this may appear as:

```text
Z:/home/eugene/gastric-pacemaker/fp-ges/build/fpga/de1-soc/fp-bootloader/de1soc_bootloader.sof
```

5. Check `Program/Configure` for the `5CSEMA5F31` device.
6. Click `Start`.

At 100%, the FPGA is running the FlexPRET bootloader.

### Reprogram Without Rebuilding

The FPGA configuration is lost when the DE1-SoC is power-cycled. If the FPGA
and bootloader source have not changed, reuse the existing `.sof` rather than
rerunning synthesis.

Open Quartus Programmer from WSL:

```bash
/mnt/c/intelFPGA/18.1/quartus/bin64/quartus_pgmw.exe
```

Select the DE1-SoC hardware, add the existing `de1soc_bootloader.sof`, check
`Program/Configure`, and click `Start`. Pressing `KEY[3]` resets FlexPRET
but does not erase the FPGA configuration.

## 2. Build the ICC Application

Use a dedicated FPGA build directory so FPGA objects and linker configuration
are not mixed with the emulator build:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cd apps/icc-model
cmake -B build-fpga
cmake --build build-fpga --target icc-model
```

The build generates:

```text
apps/icc-model/build-fpga/icc-model.mem
apps/icc-model/bin/icc-model
```

The `.mem` file is the compiled application. `bin/icc-model` is the generated
UART flash-and-console launcher.

## 3. Connect the USB-UART Adapter

The bootloader receives the ICC application through FlexPRET UART0. Use a
3.3 V USB-UART adapter and cross TX/RX:

```text
USB-UART TX  -> GPIO_0[1]  FlexPRET UART0 RX
USB-UART RX  -> GPIO_0[0]  FlexPRET UART0 TX
USB-UART GND -> GPIO_0[2]  GND
```

Do not connect a 5 V UART signal directly to the DE1-SoC GPIO.

### Attach the Adapter to WSL

If `/dev/ttyUSB0` is missing, open Windows PowerShell and run:

```powershell
usbipd list
```

Find the USB serial adapter's bus ID. Replace `1-3` below with that ID:

```powershell
usbipd bind --busid 1-3
usbipd attach --wsl --busid 1-3
```

If USBPcap requires force:

```powershell
usbipd bind --force --busid 1-3
```

Back in WSL, verify the adapter:

```bash
ls -l /dev/ttyUSB*
```

## 4. Flash and Run the ICC Application

Put the bootloader into dynamic UART loading mode:

1. Set `SW[0]` to `ON` / `1`.
2. Press `KEY[3]` to reset FlexPRET.
3. Keep `SW[0]` on while flashing.

If `SW[0]` is low during reset, the bootloader skips UART loading and runs
the static application. If `SW[0]` is high and only `LEDR0` remains on, the
bootloader is waiting for an application over UART.

From `apps/icc-model`:

```bash
sudo chmod 666 /dev/ttyUSB0
./bin/icc-model
```

The launcher serializes `build-fpga/icc-model.mem`, transfers it at 115200
baud, and opens `picocom`. The FPGA application starts the compiled five-cell
network immediately. This autonomous FPGA build does not print model data to
UART, so a blank `picocom` window after a successful transfer is expected. Exit
`picocom` with `Ctrl+A`, then `Ctrl+X`.

The Verilator target uses the same compiled configuration and prints CSV for
development testing.

## Troubleshooting

### Wrong Compiler Cached

Run `source env.bash` before configuring. If CMake was run first, it may cache
the host compiler (`/usr/bin/cc`) instead of the RISC-V compiler. Typical
errors are:

```text
cc: error: unrecognized argument in option '-mabi=ilp32'
cc: fatal error: cannot read spec file 'nosys.specs'
```

Remove only the ICC FPGA build directory and configure again:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cd apps/icc-model
rm -rf build-fpga
cmake -B build-fpga
cmake --build build-fpga --target icc-model
```

### USB-UART Permission Denied

If flashing reports `Permission denied: '/dev/ttyUSB0'`:

```bash
sudo chmod 666 /dev/ttyUSB0
./bin/icc-model
```

For a persistent fix:

```bash
sudo usermod -aG dialout $USER
```

Then run `wsl --shutdown` from Windows PowerShell, reopen WSL, and reattach
the adapter with `usbipd attach`.

### USB-UART Uses a Different Device

The device path is embedded in the launcher during CMake configuration. If the
adapter is `/dev/ttyUSB1`, set it before configuring:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
export FP_SDK_FPGA_FLASH_DEVICE=/dev/ttyUSB1
cd apps/icc-model
rm -rf build-fpga
cmake -B build-fpga
cmake --build build-fpga --target icc-model
./bin/icc-model
```

### Flash Reaches Picocom but Shows No Model Output

This is expected for the autonomous FPGA build: UART is used by the bootloader
to load the application, but the running ICC model does not transmit telemetry.
Use the Verilator target when CSV output is required.

If the application transfer itself fails, check that:

- the bootloader `.sof` is programmed;
- `SW[0]` was on when `KEY[3]` was pressed;
- USB-UART TX and RX are crossed;
- the adapter uses 3.3 V logic; and
- the launcher uses the correct `/dev/ttyUSB*` device.

## Numerical Representation and Output

The reference model expresses voltage in millivolts. Before compilation, each
voltage is converted to an integer number of nanovolts:

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
use signed integers. The source increments are calibrated for the production
200 ms timestep. Experimental smaller-step Q0 increments use a calibrated
compile-time lookup table. Q1-Q3 increments remain integer-scaled. The runtime
update contains no floating-point values, multiplication, division, or
square-root operations.
Diagnostic conversion to nearest whole microvolts uses integer division only
for CSV output.

Both targets use Cell 4 as a 20-second pacemaker, four follower cells, 1000 ms
path delays, and 6 mm gaps. Path delay and active propagation time remain
integer milliseconds.

## Configure the Autonomous 1D Network

The network parameters are compiled into `src/main.c`. Change the three arrays
inside `initialize_network()` to configure the cell intervals, path delays, and
path gaps:

```c
static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
    0, 0, 0, 0, 20
};

static const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
    1000U, 1000U, 1000U, 1000U
};

static const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {
    6U, 6U, 6U, 6U
};
```

`intervals[i]` configures Cell `i`. Path `i` joins Cell `i` to Cell `i + 1`,
so `delays[i]` and `gaps[i]` configure that connection. The supported cell
intervals are:

| Value | Meaning |
|---:|---|
| `-1` | Blocked cell |
| `0` | Follower with no intrinsic pacemaker |
| `15`, `20`, `23`, `26`, `30`, `40` | Pacemaker interval in seconds |

Every path delay must be greater than the selected model timestep and an exact
multiple of that timestep. With the production 200 ms timestep, valid examples
include 400, 600, 800, 1000, and 1200 ms. Each gap must be from 1 to 255 mm.

For example, this makes Cell 0 the pacemaker and gives every path different
parameters:

```c
static const int8_t intervals[ICC_NETWORK_1D_CELL_COUNT] = {
    20, 0, 0, 0, 0
};

static const uint16_t delays[ICC_NETWORK_1D_PATH_COUNT] = {
    400U, 600U, 800U, 1000U
};

static const uint8_t gaps[ICC_NETWORK_1D_PATH_COUNT] = {
    3U, 4U, 5U, 6U
};
```

To change the number of cells, edit `inc/network.h`:

```c
#define ICC_NETWORK_1D_CELL_COUNT 7U
#define ICC_NETWORK_1D_PATH_COUNT (ICC_NETWORK_1D_CELL_COUNT - 1U)
```

The path count is derived automatically. After changing the cell count, update
the arrays in `src/main.c` so they contain one interval per cell and one delay
and gap per path. The construction and stepping loops in `src/network.c`
already use these compile-time counts and do not need to be rewritten.

The emulator CSV functions `print_csv_header()` and `print_csv_row()` currently
describe exactly five cells and four paths. They must also be updated when the
cell count changes. With fewer than five cells, the existing output code could
access outside the arrays; with more than five cells, it would omit the extra
cells. Update the five-cell expectations in `tests/test_icc.c` as well.

The defaults in `inc/path.h` do not override these arrays. The autonomous
network passes every delay and gap explicitly from `src/main.c`. Adding a new
pacemaker interval outside the supported list also requires adding its
calibrated resting increments in `inc/icc_calibration.h` and a validation case.

## Experimental Timestep Selection

The production timestep is 200 ms. A smaller compile-time timestep can be
selected for sensitivity testing:

```bash
cmake -S . -B build-emu-100ms \
  -DTARGET=emulator \
  -DICC_MODEL_TIMESTEP_MS=100
cmake --build build-emu-100ms --target icc-model
```

`ICC_MODEL_TIMESTEP_MS` must be no greater than 200 and must divide 200
exactly. The tested values are 200, 100, 50, 20, and 10 ms. The Q0 calibration
lookup and Q1-Q3 scaling use integer arithmetic and introduce no floating point.

At 200, 100, and 50 ms, the calibration reproduces every configured intrinsic
cpm exactly. The maximum measured error at 20 and 10 ms is 20 ms because
some target periods have no exact constant integer Q0 increment. Smaller modes
remain experimental; see
[TIMESTEP_TEST_RESULTS.md](TIMESTEP_TEST_RESULTS.md) before using them in a
physiological result.

## 5. Build and Run in the Emulator

The emulator uses FlexPRET's Verilator-based `fp-emu`. Keep it in a separate
build directory:

```bash
cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cd apps/icc-model
cmake -B build-emu -DTARGET=emulator
cmake --build build-emu --target icc-model
./bin/icc-model-emu
```

This is equivalent to:

```bash
fp-emu +ispm=apps/icc-model/build-emu/icc-model.mem
```

Stop the continuously running model with `Ctrl+C`. Check that the emulator is
available with:

```bash
fp-emu --hwconfig
```

## 6. Run the Host Test

```bash
cd /home/eugene/gastric-pacemaker/fp-ges/apps/icc-model
gcc -std=c11 -Wall -Wextra -Werror -Iinc \
    src/icc.c src/path.c src/network.c tests/test_icc.c -o /tmp/icc-model-test
/tmp/icc-model-test
```

## Target Configuration Reference

- `TARGET` accepts `fpga` or `emulator`.
- The default is `fpga`.
- `ICC_MODEL_TIMESTEP_MS` defaults to `200`; tested experimental values are
  `100`, `50`, `20`, and `10`.
- FPGA and emulator builds use separate build directories.
- `bin/icc-model` is the FPGA launcher.
- `bin/icc-model-emu` is the emulator launcher.
- Five cells and four bidirectional paths are included; EGM is not implemented.

## Short Version

```bash
# First-time setup or after changing FPGA/bootloader source:
# In VS Code, run: Mega: Build and Synthesize Bootloader FPGA
# In Quartus Programmer, select DE-SoC [USB-1], add:
# build/fpga/de1-soc/fp-bootloader/de1soc_bootloader.sof
# Check Program/Configure and click Start.
#
# After a power cycle, reuse the existing .sof in Quartus Programmer.
#
# Wire the 3.3 V USB-UART:
# TX -> GPIO_0[1], RX -> GPIO_0[0], GND -> GPIO_0[2]

cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
export RISCV_TOOL_PATH_PREFIX=/opt/xpack-riscv-none-elf-gcc-14.2.0-2
cd apps/icc-model
cmake -B build-fpga
cmake --build build-fpga --target icc-model

# On the board: set SW[0] on, press KEY[3], and leave SW[0] on.
sudo chmod 666 /dev/ttyUSB0
./bin/icc-model

# Exit picocom with Ctrl+A, then Ctrl+X.
```
