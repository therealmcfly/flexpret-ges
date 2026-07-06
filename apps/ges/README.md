# GES on FlexPRET FPGA

This app is built for running on FlexPRET on the DE1-SoC FPGA with the
FlexPRET bootloader. The bootloader is programmed onto the FPGA first, then the
GES program is sent to instruction scratchpad memory over UART.

## 1. Build and Program the Bootloader FPGA Image

From VS Code, run:

```text
Ctrl+Shift+P
Tasks: Run Task
Mega: Build and Synthesize Bootloader FPGA
```

This task runs these steps in sequence:

1. Build the bootloader SDK.
2. Generate the DE1-SoC FlexPRET Verilog.
3. Run the Quartus bootloader project flow.
4. Open Quartus Programmer.

When Quartus Programmer opens, select the DE1-SoC FPGA/USB-Blaster target and
program the generated bootloader image onto the board.

In Quartus Programmer:

1. Connect the DE1-SoC USB-Blaster cable to the PC if it is not already
   connected.
2. Check the hardware field near the top of the window.
3. If Quartus does not auto-detect the board, click `Hardware Setup...` and
   select `DE-SoC [USB-1]`.
4. Keep `Mode` set to `JTAG`.
5. Click `Add File...`.
6. Select the generated bootloader `.sof` file:

```text
build/fpga/de1-soc/fp-bootloader/de1soc_bootloader.sof
```

In the Windows Quartus file picker this may appear through the WSL mount as:

```text
Z:/home/eugene/gastric-pacemaker/fp-ges/build/fpga/de1-soc/fp-bootloader/de1soc_bootloader.sof
```

7. Check `Program/Configure` for the `5CSEMA5F31` device row. This must be
   checked before `Start` will program the FPGA.
8. Click `Start`.

When programming completes, the FPGA is loaded with the FlexPRET bootloader.

### Reprogram FlexPRET Without Rebuilding or Synthesizing

If the DE1-SoC has been power-cycled and the FPGA source has not changed, do
not rerun the full Mega task. Open Quartus Programmer directly from WSL:

```bash
/mnt/c/intelFPGA/18.1/quartus/bin64/quartus_pgmw.exe
```

In Quartus Programmer, select the DE1-SoC USB-Blaster hardware and add the
existing synthesized image:

```text
Z:/home/eugene/gastric-pacemaker/fp-ges/build/fpga/de1-soc/fp-bootloader/de1soc_bootloader.sof
```

Check `Program/Configure`, then click `Start`. Rerun
`Mega: Build and Synthesize Bootloader FPGA` only after changing the FPGA or
bootloader source. Pressing `KEY3` resets FlexPRET but does not erase the FPGA
configuration, so it does not require reprogramming or synthesis.

## 2. Build GES

From the repository root:

```bash
source env.bash
cd apps/ges
cmake -B build
cmake --build build --target ges

rm -rf bin build && cmake -B build && cmake --build build

```

The build creates:

```text
apps/ges/build/ges.mem
apps/ges/bin/ges
```

`ges.mem` is the compiled program. `bin/ges` is the generated flash script.

## 3. Flash GES to the Bootloader

Before running the flash script, put the bootloader in dynamic UART loading
mode:

1. Set `SW[0]` to `ON` / `1`.
2. Press `KEY[3]` to reset FlexPRET.
3. Keep `SW[0]` set while flashing.

The bootloader checks `SW[0]` during startup. If `SW[0]` is low when FlexPRET
resets, it skips UART loading and runs the static app already in memory.

After the FPGA is programmed, reset, and running the bootloader in dynamic-load
mode:

```bash
cd apps/ges
./bin/ges
```

The generated script serializes `build/ges.mem`, sends it over UART, then opens
`picocom`:

```text
python3 scripts/serialize_app.py
python3 scripts/send_uart.py
picocom
```

By default this repo uses:

```text
UART device: /dev/ttyUSB0
Baud rate:   115200
```

These defaults come from `env.bash`.

## WSL USB-UART Setup

If `./bin/ges` cannot find `/dev/ttyUSB0`, attach the USB-UART adapter to WSL
from Windows PowerShell.

First list USB devices:

```powershell
usbipd list
```

Find the USB serial adapter, for example:

```text
1-3    0403:6001  USB Serial Converter  Shared
```

If it is not shared, bind it:

```powershell
usbipd bind --busid 1-3
```

If `usbipd` reports that `USBPcap` requires force, use:

```powershell
usbipd bind --force --busid 1-3
```

Then attach it to WSL:

```powershell
usbipd attach --wsl --busid 1-3
```

Back in WSL, verify that the device exists:

```bash
ls -l /dev/ttyUSB*
```

The GES flash script expects `/dev/ttyUSB0`.

## DE1-SoC UART0 Wiring

The bootloader receives the app over UART0. The USB-UART adapter must be wired
to the DE1-SoC GPIO header with TX/RX crossed:

```text
USB-UART TX  -> GPIO_0[1]  FlexPRET UART0 RX
USB-UART RX  -> GPIO_0[0]  FlexPRET UART0 TX
USB-UART GND -> GPIO_0[2]  GND
```

Use a 3.3 V USB-UART adapter.

If `SW[0]` is high and only `LEDR0` stays on, FlexPRET is running the
bootloader and waiting for UART input. If `./bin/ges` reaches `picocom` but the
board does not react, check that TX and RX are crossed correctly.

## USB-UART Permission Fix

If flashing fails with:

```text
Permission denied: '/dev/ttyUSB0'
```

use the quick permission fix:

```bash
sudo chmod 666 /dev/ttyUSB0
```

Then rerun:

```bash
./bin/ges
```

The more permanent fix is to add your user to the `dialout` group:

```bash
sudo usermod -aG dialout $USER
```

After that, restart WSL from Windows PowerShell:

```powershell
wsl --shutdown
```

Then reopen WSL, reattach the USB-UART with `usbipd attach`, and run
`./bin/ges` again.

## Exiting Picocom

After flashing succeeds, `./bin/ges` opens `picocom` on the UART.

To exit:

```text
Ctrl-a
Ctrl-x
```

Press `Ctrl-a`, release both keys, then press `Ctrl-x`.

## Changing the UART Device

If the board appears as a different device, set `FP_SDK_FPGA_FLASH_DEVICE`
before configuring/building GES:

```bash
source env.bash
export FP_SDK_FPGA_FLASH_DEVICE=/dev/ttyUSB1

cd apps/ges
cmake -B build
cmake --build build --target ges
./bin/ges
```

The device path is baked into `bin/ges` when CMake generates the script, so
rerun CMake after changing `FP_SDK_FPGA_FLASH_DEVICE`.

## Short Version

```bash
# In VS Code:
# Run task: Mega: Build and Synthesize Bootloader FPGA
# In Quartus Programmer:
# Select DE-SoC [USB-1], add de1soc_bootloader.sof, check Program/Configure,
# then press Start.
#
# On the DE1-SoC:
# Set SW[0] = ON/1, then press KEY[3] to reset into dynamic UART loading mode.
# Wire UART0 as:
# USB-UART TX -> GPIO_0[1], USB-UART RX -> GPIO_0[0], GND -> GPIO_0[2].

cd /home/eugene/gastric-pacemaker/fp-ges
source env.bash
cd apps/ges
cmake -B build
cmake --build build --target ges
sudo chmod 666 /dev/ttyUSB0
./bin/ges

# To exit picocom after flashing:
# Ctrl-a, then Ctrl-x
```
