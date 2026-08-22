# Hardware-in-the-Loop Setup

This guide describes the hardware-in-the-loop (HIL) connection between:

- the FlexPRET gastric electrical stimulator (GES) running on the DE1-SoC FPGA; and
- the embedded ICC RiSPA model running on an Arduino Mega and configured by the RiSPA controller application.

## Communication Overview

The DE1-SoC and Arduino Mega communicate through UART1 at 115200 baud.

| Direction | Source | Destination | Data |
|---|---|---|---|
| ICC to GES | Mega TX1 | DE1-SoC UART1 RX | Signed 16-bit EGM samples |
| GES to ICC | DE1-SoC UART1 TX | Mega RX1 | Pacing command byte |

The Arduino uses `Serial1`:

- TX1: Mega pin 18
- RX1: Mega pin 19

The FlexPRET top-level design uses these logical signals:

- `GPIO_0[3]`: UART1 TX, exposed on GPIO_0 physical header pin 4
- `GPIO_0[4]`: UART1 RX, exposed on GPIO_0 physical header pin 5

## Physical Wiring

Power off both boards before changing the wiring.

### DE1-SoC TX to Mega RX

The DE1-SoC transmits 3.3 V logic. The Mega can receive this directly:

```text
DE1-SoC GPIO_0 physical pin 4 (UART1 TX)
    |
    +---------------- Mega pin 19 (RX1)
```

### Mega TX to DE1-SoC RX

The Mega transmits 5 V logic, while the DE1-SoC GPIO input uses 3.3 V logic. Use a resistor divider made from three 220 ohm resistors:

```text
Mega pin 18 (TX1)
    |
  220 ohm
    |
    +---------------- DE1-SoC GPIO_0 physical pin 5 (UART1 RX)
    |
  220 ohm
    |
  220 ohm
    |
Mega GND ------------ DE1-SoC GPIO_0 physical pin 12 (GND)
```

The lower two 220 ohm resistors must be connected in series. Together they provide 440 ohm to ground:

```text
5 V x 440 / (220 + 440) = approximately 3.33 V
```

### Common Ground

Connect an Arduino Mega GND pin to DE1-SoC GPIO_0 physical header pin 12, which is a dedicated ground pin.

Do not use GPIO_0 physical pin 6 (`GPIO_0[5]`) as ground. The current Verilog drives that programmable FPGA signal low, but it is not a dedicated board-ground connection.

## Software and Startup Sequence

Use this order to avoid stale pacing commands from a previous GES run.

1. Build and synthesize the FlexPRET bootloader design for the DE1-SoC.
2. Program the synthesized FlexPRET design onto the DE1-SoC FPGA.
3. Flash the GES application to FlexPRET instruction memory (ISPM).
4. Connect the embedded ICC RiSPA Arduino Mega to the PC.
5. Run the RiSPA controller application and connect it to the Mega serial port.
6. Reset the FlexPRET GES by pressing `KEY3` on the DE1-SoC.
7. Confirm that the GES has restarted in its initial learning state.
8. Configure the RiSPA model:
   - grid dimensions;
   - simulation timestep;
   - ICC activation intervals;
   - horizontal and vertical path delays and gaps;
   - EGM electrode locations and heights;
   - the GES sensing electrode; and
   - the pacing-lead location.
9. Ensure the RiSPA simulation timestep matches `SAMPLING_INTERVAL_MS` in `apps/ges/inc/global.h`.
10. Press **Initialize Board** in the RiSPA controller.

The GES sensing electrode must be configured for FlexPRET to receive EGM samples. The pacing lead must be configured for pacing commands from FlexPRET to stimulate a cell.

## Expected Runtime Flow

1. The Mega simulates the ICC network.
2. The selected sensing electrode calculates an EGM sample.
3. The Mega sends the scaled signed EGM sample from TX1 to FlexPRET UART1 RX.
4. FlexPRET processes the EGM and advances its GES state machine.
5. While the GES remains in the pacing state, it sends `AA 55 01` from UART1 TX.
6. The receiving implementation must accept only a complete frame with a
   payload value of `1` before stimulating the configured pacing-lead cell.
   The legacy Mega receiver must be updated from its raw-byte-`1` protocol
   before this workflow is used with the framed GES build.
7. The paced activation propagates through the ICC network and contributes to later EGM samples.

## Troubleshooting

### Unexpected activation immediately after initialization

Reset the FlexPRET GES with `KEY3` before initializing the Mega. A previous GES run may still be in its pacing state and continue transmitting pacing commands.

The Mega also drains queued UART1 pacing bytes immediately before initializing the ICC network.

### Values such as 255 and -256

These values can indicate UART byte corruption or one-byte misalignment. Check:

- that the lower divider resistors are in series, not parallel;
- that Mega GND is connected to a dedicated DE1-SoC GND pin;
- that Mega TX1 passes through the 5 V-to-3.3 V divider;
- that TX and RX are crossed correctly; and
- that both UARTs use 115200 baud.

### GES timing is too fast or too slow

The GES advances its software timers by `SAMPLING_INTERVAL_MS` for each received sample. This value must equal the RiSPA controller timestep.
