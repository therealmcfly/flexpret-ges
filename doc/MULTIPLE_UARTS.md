# UART extension implementation log — FlexPRET / DE1-SoC

This document records how the original FlexPRET UART interface was extended to
UART0–UART3. It consolidates the earlier UART1/UART2 notes and the UART3 source
changes inspected on 2026-10-13. Historical changes below are described from the
earlier notes; the current source was checked to distinguish completed edits from
pending application changes and hardware verification.

## Purpose

The original UART0 was used for bootloader uploads and console output. UART1 and
UART2 were added for communication with RiSPA and GES. UART3 was then added to
provide a separate connection for ICC recording, allowing picocom to remain on
UART0 while another USB–UART adapter receives binary log data.

| UART | Intended ICC-model role |
| --- | --- |
| UART0 | Bootloader uploads and debug console |
| UART1 | RiSPA hardware bridge, by default |
| UART2 | GES EGM transmission and pacing reception, by default |
| UART3 | Dedicated ICC log transmission to the PC |

RiSPA and GES UART selection is configurable. Adding UART3 hardware does not
automatically move the application's log sender to it.

## Earlier extension: UART1 and UART2

### Peripheral instances and connections

In src/main/scala/Core/Top.scala, the original single UART was renamed wbUart0,
and wbUart1 and wbUart2 were instantiated using the existing WishboneUart module.
The Wishbone device list became Seq(4, 4, 4), and devices 0–2 were connected to
the corresponding UART instances. FpgaTopIO and FpgaTop were extended with uart1
and uart2 RX/TX bundles and connections.

No separate UART peripheral implementation was required; each instance reused
src/main/scala/Wishbone/WishboneUart.scala.

The SDK uart.h gained UART1_BASE=0x0010UL and UART2_BASE=0x0020UL. The existing
uart_send()/uart_receive() functions already accepted a base address.

### Initial emulator treatment

Because AbstractTop also instantiated the additional UARTs in VerilatorTop,
their unused receiver inputs needed explicit values. The earlier implementation
tied RX LOW and assigned DontCare to unused TX outputs. This was subsequently
replaced during the UART3 work, as recorded below.

### Initial address-width workaround

The old WishboneBusIO check was:

```scala
require(masterWidth > deviceWidths.sum)
```

With three four-bit local device addresses, that expression required masterWidth
to exceed 12. busAddrBits was increased from 10 to 16 in FlexpretConfiguration,
located in src/main/scala/Core/config.scala. That allowed the old check to pass.

Later review established that this was a workaround for an incorrect capacity
check: three UARTs occupy 48 bytes, not 12 address bits. The corrected check was
introduced with UART3 rather than increasing the bus width again.

## Board-routing revision before UART3

The earlier Top.v mapped UART0 to GPIO_0[0:1], UART1 to [3:4] and UART2 to [6:7].
GPIO_0[2], [5] and [8] were driven LOW and commented as ground.

Those were programmable signal outputs, not dedicated GND pins. The constant-LOW
assignments were removed and the UART signals were packed consecutively:

| Interface | Earlier TX/RX | Revised TX/RX |
| --- | --- | --- |
| UART0 | [0] / [1] | [0] / [1] |
| UART1 | [3] / [4] | [2] / [3] |
| UART2 | [6] / [7] | [4] / [5] |

FlexPRET GPIO output port 3 was also separated from the LEDs. Ports 0–2 retained
LED0–LED5; port 3 used a separate two-bit sync_out wire, initially with bit 0
routed to GPIO_0[6]. LED6–LED9 were explicitly held off.

Shared ground connections were identified as belonging on dedicated board GND,
such as GPIO_0 physical header contact 12, not the former LOW-driven signals.
Physical connector numbering and GPIO signal indexing are separate.

## UART3 extension — source inspected 2026-10-13

### Fourth UART and Wishbone device

In src/main/scala/Core/Top.scala, wbUart3 was added and the bus was extended to
four devices:

```scala
val wbUart3 = Module(new WishboneUart()(cfg))
val wbBus = Module(new WishboneBus(cfg.busAddrBits, Seq(4, 4, 4, 4)))
wbBus.io.wbDevices(3) <> wbUart3.io.port
```

FpgaTopIO gained the uart3 bundle. FpgaTop connected it to the new peripheral:

```scala
io.uart3.tx := wbUart3.ioUart.tx
wbUart3.ioUart.rx := io.uart3.rx
```

These additions expose io_uart3_tx and io_uart3_rx when Chisel generates the
FPGA top-level module.

### Corrected address-space validation

In src/main/scala/Wishbone/WishboneBus.scala, the old width-summing check was
replaced with an address-range capacity check:

```scala
require(
    (BigInt(1) << masterWidth) >=
    deviceWidths.map(w => BigInt(1) << w).sum
)
```

Four UARTs occupy 64 bytes. The previous expression would reject masterWidth=16
because 16 > 16 is false, despite the available address space being sufficient.
The revised expression compares 65536 available addresses against 64 required.
The existing busAddrBits=16 was retained.

### Emulator receivers set to idle

VerilatorTop retained its exposed UART0 interface. UART1–UART3 remained internal
and their receiver inputs were changed to HIGH, the UART idle level:

```scala
wbUart1.ioUart.rx := true.B
wbUart2.ioUart.rx := true.B
wbUart3.ioUart.rx := true.B
```

Unused TX outputs were left unconnected instead of assigning DontCare. This
connects the required inputs; it does not implement host serial connections or
end-to-end emulator tests for UART1–UART3.

### SDK address added

sdk/lib/include/flexpret/uart.h gained:

```c
#define UART3_BASE 0x0030UL
```

The peripheral order gives this address map:

| UART | Address range | Base constant |
| --- | --- | --- |
| UART0 | 0x0000–0x000F | UART0_BASE = 0x0000UL |
| UART1 | 0x0010–0x001F | UART1_BASE = 0x0010UL |
| UART2 | 0x0020–0x002F | UART2_BASE = 0x0020UL |
| UART3 | 0x0030–0x003F | UART3_BASE = 0x0030UL |

TXD, RXD, CSR and CONST offsets remained 0x0, 0x4, 0x8 and 0xC. No new software
send function was needed in the SDK; uart_send(UART3_BASE, byte) selects UART3.

### Board wrapper extended and sync moved

fpga/de1-soc/fp-bootloader/rtl/Top.v gained UART3 wires, header assignments and
FpgaTop instance connections. UART3 took GPIO_0[6:7], and synchronization moved
from GPIO_0[6] to GPIO_0[8]:

```verilog
assign GPIO_0[6] = uart3_tx;
assign uart3_rx = GPIO_0[7];
assign GPIO_0[8] = sync_out[0];

// Inside FpgaTop flexpret(...):
.io_uart3_tx(uart3_tx),
.io_uart3_rx(uart3_rx),
```

The unused GPIO loop was moved to start at index 9. Existing QSF assignments
already covered these GPIO signals, so no additional package pin assignment was
needed for this mapping.

| Function | Current TX/output | Current RX |
| --- | --- | --- |
| UART0 | GPIO_0[0] | GPIO_0[1] |
| UART1 | GPIO_0[2] | GPIO_0[3] |
| UART2 | GPIO_0[4] | GPIO_0[5] |
| UART3 | GPIO_0[6] | GPIO_0[7] |
| Reserved synchronization | GPIO_0[8] | — |

The inspected source connects UART3 RX to the header. A transmit-only installation
must either provide an idle RX level or tie the generated RX input HIGH in Top.v;
an active receiver should not be left floating. This optional tie-off was discussed
but was not present in the inspected source.

## Build and verification status

At documentation time, the user reported the hardware build was in progress.
The assistant inspected source files; it did not rerun synthesis, program the
new UART3 image, or perform UART3 serial tests while writing this log.

The existing VS Code task Mega: Build and Synthesize Bootloader FPGA sequences:

1. Build bootloader sdk.
2. Build FlexPRET FPGA, including generate_verilog_de1soc.
3. Synthesize Bootloader Project FPGA and open Quartus Programmer.

The task uses Quartus under /mnt/c/intelFPGA_lite/18.1/quartus/bin64 on this
workstation. Regenerating Chisel output and programming the resulting .sof are
required; rebuilding only the application cannot add a hardware UART.

A build-system limitation was observed during inspection: generate_verilog_de1soc
uses FpgaTop.v as its output without declaring all Scala files as dependencies.
Consequently, an existing generated file can appear up-to-date after a Scala edit.
Force regeneration if needed and verify generated UART3 ports before synthesis.
This limitation was documented, not repaired in this change.

## Remaining work at the time of this log

| Item | Status |
| --- | --- |
| Four UART instances and FPGA interface connections | Present in inspected source |
| Corrected Wishbone capacity check | Present in inspected source |
| Idle emulator receiver inputs | Present in inspected source |
| UART3_BASE in SDK | Present in inspected source |
| UART3 header routing and sync relocation | Present in inspected source |
| New Verilog generation and Quartus build | User reported building; result not independently checked |
| Programming the new UART3 FPGA image | Not confirmed for this revision |
| ICC-model send_log_data() changed to UART3 | Pending: inspected source still uses UART0_BASE |
| ICC-model startup transmission message changed to UART3 | Pending: inspected source still reports UART0 |
| UART3 physical log capture | Not tested |
| Deadline behavior with all UART transmissions | Not verified |

UART3 is intended to use another 3.3 V USB–UART adapter. Its ground must connect
to dedicated board GND. The PC recorder can then use that adapter while picocom
uses the UART0 adapter. Existing packet encoding does not change with the UART
destination. Separate UART hardware does not eliminate software transmission time;
sustained acquisition and model deadlines still need verification.
