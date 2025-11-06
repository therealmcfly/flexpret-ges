#!/usr/bin/tclsh

# Quartus build script for FlexPRET DE1-SoC
package require ::quartus::project
package require ::quartus::flow

# Variables
set projectName "de1soc_blinky"
set part "5CSEMA5F31C6"
set outputDir "."

puts "Setting up FlexPRET for DE1-SoC in: [pwd]"

# Create new project
project_new -overwrite $projectName

# Set device
set_global_assignment -name FAMILY "Cyclone V"
set_global_assignment -name DEVICE $part
set_global_assignment -name TOP_LEVEL_ENTITY "Top"

puts "Adding design sources"

# Add all Verilog files from rtl directory
foreach vfile [glob -nocomplain rtl/*.v] {
    puts "Adding source file: $vfile"
    set_global_assignment -name VERILOG_FILE $vfile
}

# Add generated Verilog files from build directory
set buildVerilogDir "../../../../build/fpga/de1-soc"
if {[file exists "$buildVerilogDir/FpgaTop.v"]} {
    puts "Adding generated file: $buildVerilogDir/FpgaTop.v"
    set_global_assignment -name VERILOG_FILE "$buildVerilogDir/FpgaTop.v"
}

# Add DualPortBram for FPGA
set dualPortBramFile "../../../../src/main/resources/DualPortBramFPGA.v"
if {[file exists $dualPortBramFile]} {
    puts "Adding FPGA BRAM file: $dualPortBramFile"
    set_global_assignment -name VERILOG_FILE $dualPortBramFile
}

puts "Adding constraints"

# Add SDC constraints
#if {[file exists ../xdc/de1-soc.sdc]} {
#    puts "Adding constraints file: ../xdc/de1-soc.sdc"
#    set_global_assignment -name SDC_FILE ../xdc/de1-soc.sdc
#}

puts "Setting compilation options"

# Set compilation options
set_global_assignment -name STRATIX_DEVICE_IO_STANDARD "3.3-V LVTTL"
set_global_assignment -name CYCLONEII_RESERVE_NCEO_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DATA0_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DATA1_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_FLASH_NCE_AFTER_CONFIGURATION "USE AS REGULAR IO"
set_global_assignment -name RESERVE_DCLK_AFTER_CONFIGURATION "USE AS REGULAR IO"

source ../xdc/DE1_SoC.qsf


# Save project
project_close

puts "Project setup complete!"
puts "To compile the design, run:"
puts "  quartus_sh --flow compile $projectName"
puts "Or use the run_build.tcl script"
