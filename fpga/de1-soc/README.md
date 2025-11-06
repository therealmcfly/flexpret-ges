# FlexPRET DE1-SoC FPGA Build

This directory contains all the files needed to synthesize and program the FlexPRET processor onto a DE1-SoC FPGA board.

## Files Structure

```
build/fpga/de1-soc/
├── rtl/                    # Verilog source files
│   ├── FpgaTop.v          # Main FlexPRET processor (generated)
│   ├── Top.v              # DE1-SoC board wrapper
│   └── DualPortBramFPGA.v # Dual-port RAM module
├── xdc/                   # Constraints files
│   └── de1-soc.sdc        # Pin constraints and timing
├── tcl/                   # TCL scripts (original templates)
├── setup_project.tcl      # Quartus project setup script
├── run_build.tcl          # Quartus compilation script
├── build_flexpret.sh      # Linux/macOS build script
├── build_flexpret.bat     # Windows build script
└── README.md              # This file
```

## Prerequisites

1. **Intel Quartus Prime** (tested with Quartus Prime Lite)
2. **DE1-SoC Development Board**
3. **USB-Blaster** (for programming)

## Quick Start

### Option 1: Automated Build (Recommended)

**Windows:**
```cmd
cd build\fpga\de1-soc
build_flexpret.bat
```

**Linux/macOS:**
```bash
cd build/fpga/de1-soc
./build_flexpret.sh
```

### Option 2: Manual Steps

1. **Setup Quartus Project:**
   ```
   quartus_sh -t setup_project.tcl
   ```

2. **Run Compilation:**
   ```
   quartus_sh -t run_build.tcl
   ```

3. **Or open in Quartus GUI:**
   ```
   quartus flexpret_de1soc.qpf
   ```

## Programming the Board

After successful compilation:

1. Connect DE1-SoC board via USB-Blaster
2. Open Quartus Programmer
3. Load `flexpret_de1soc.sof`
4. Program the device

## Board Interface

The FlexPRET processor is mapped to DE1-SoC as follows:

| FlexPRET Signal | DE1-SoC Pin | Description |
|----------------|-------------|-------------|
| CLOCK_50       | PIN_AF14    | 50MHz system clock |
| RESET          | KEY[0]      | Active-high reset |
| BTNS[3:0]      | KEY[3:1]    | Input buttons |
| LEDS[7:0]      | LED[7:0]    | Output LEDs |
| UART_TX        | GPIO[0]     | UART transmit |
| UART_RX        | GPIO[1]     | UART receive |

## FlexPRET Configuration

This build uses:
- 50MHz system clock (direct from board oscillator)
- 8 LEDs for GPIO output
- 4 buttons for GPIO input
- UART interface on GPIO pins

## Troubleshooting

### Common Issues:

1. **"quartus_sh not found"**
   - Ensure Quartus is installed and in PATH
   - On Windows: Use "Quartus Prime Command Prompt"

2. **"FpgaTop.v not found"**
   - Run the Verilog generation first: `make generate_verilog_de1soc`
   - Ensure you're in the correct directory

3. **Compilation errors**
   - Check `output/*.rpt` files for detailed error messages
   - Verify all .v files are present in `rtl/` directory

4. **Programming fails**
   - Check USB-Blaster connection
   - Verify DE1-SoC power and JTAG chain

### Getting Help

- Check Quartus compilation reports in `output/` directory
- Review pin assignments in `de1-soc.sdc`
- Verify FlexPRET configuration matches your requirements

## Generated Files

After successful compilation:
- `flexpret_de1soc.sof` - SRAM Object File (for JTAG programming)
- `flexpret_de1soc.pof` - Programming Object File (for flash)
- `output/` directory contains detailed reports

## Next Steps

Once programmed, the FlexPRET processor will:
1. Boot from internal memory
2. Control LEDs based on programmed software
3. Respond to button presses
4. Communicate via UART (if software supports it)

To load and run programs on FlexPRET, see the `sdk/` directory for software development tools.
