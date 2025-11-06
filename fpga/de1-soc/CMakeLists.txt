set(BOARD_NAME de1-soc)
set(PART_SHORT 5CSEMA5F31C6)
set(PART 5CSEMA5F31C6N)

set(FP_FPGA_BOARD_PATH ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(fp-bootloader)
# add_subdirectory(fp-blinky)
# add_subdirectory(fp-rfu)
# add_subdirectory(leds)

# add_dependencies(copy_ispm_mem)

# Ensure output directory exists before running sbt
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/fpga/de1-soc)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/fpga/de1-soc/rtl)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/fpga/de1-soc/xdc)

# Copy all helper files and scripts to build directory
# configure_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/setup_project.tcl
#   ${CMAKE_BINARY_DIR}/fpga/de1-soc/setup_project.tcl
#   COPYONLY
# )

#configure_file(
#  ${CMAKE_CURRENT_SOURCE_DIR}/run_build.tcl
#  ${CMAKE_BINARY_DIR}/fpga/de1-soc/run_build.tcl
#  COPYONLY
#)

# configure_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/build_flexpret.sh
#   ${CMAKE_BINARY_DIR}/fpga/de1-soc/build_flexpret.sh
#   COPYONLY
# )

# configure_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/README.md
#   ${CMAKE_BINARY_DIR}/fpga/de1-soc/README.md
#   COPYONLY
# )

# Copy constraints file
# configure_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/xdc/de1-soc.sdc
#   ${CMAKE_BINARY_DIR}/fpga/de1-soc/xdc/de1-soc.sdc
#   COPYONLY
# )
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/xdc/DE1_SoC.qsf
  ${CMAKE_BINARY_DIR}/fpga/de1-soc/xdc/DE1_SoC.qsf
  COPYONLY
)


# Copy Top.v wrapper
# configure_file(
#   ${CMAKE_CURRENT_SOURCE_DIR}/rtl/Top.v
#   ${CMAKE_BINARY_DIR}/fpga/de1-soc/rtl/Top.v
#   COPYONLY
# )

# Copy DualPortBram from resources
configure_file(
  ${CMAKE_SOURCE_DIR}/src/main/resources/DualPortBramFPGA.v
  ${CMAKE_BINARY_DIR}/fpga/de1-soc/rtl/DualPortBram.v
  COPYONLY
)

#Generates the Verilog from Scala
add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/fpga/de1-soc/FpgaTop.v
  COMMAND ${CMAKE_COMMAND} -E echo "Generating Verilog for DE1-SoC"
  COMMAND sbt 'run fpga h0 --no-dedup --target-dir ${CMAKE_BINARY_DIR}/fpga/de1-soc'
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  COMMENT "Generating Verilog for DE1-SoC (run from project root)"
)

add_custom_target(
  generate_verilog_de1soc
  DEPENDS ${CMAKE_BINARY_DIR}/fpga/de1-soc/FpgaTop.v
)

# Add a target that copies all files and generates Verilog
add_custom_target(
  prepare_de1soc_build
  DEPENDS generate_verilog_de1soc
  COMMENT "Preparing complete DE1-SoC build with all files"
)
