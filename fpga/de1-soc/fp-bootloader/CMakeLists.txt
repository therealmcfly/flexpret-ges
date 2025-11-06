# CMake build for DE1-SoC fp-bootloader (Quartus flow)
set(TOP_LEVEL Top)
set(QSF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../xdc/DE1_SoC.qsf)
set(VERILOG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rtl/Top.v
    # Add other RTL files as needed
)

# Copy RTL files to build directory
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rtl/Top.v
  ${CMAKE_CURRENT_BINARY_DIR}/rtl/Top.v
  COPYONLY
)

# Copy TCL setup script to build directory
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/tcl/setup_bootloader_project.tcl
  ${CMAKE_CURRENT_BINARY_DIR}/tcl/setup_bootloader_project.tcl
  COPYONLY
)

# Copy bootloader .mem file to build directory
configure_file(
  ${CMAKE_SOURCE_DIR}/sdk/build/bootloader/bootloader.mem
  ${CMAKE_BINARY_DIR}/fpga/de1-soc/fp-bootloader/ispm.mem
  COPYONLY
)

# set(TARGET_NAME fp-fpga-bootloader-de1soc)

# add_custom_target(${TARGET_NAME} 
#   COMMAND "quartus_sh" 
#     "-t" "${CMAKE_BINARY_DIR}/fpga/de1-soc/fp-bootloader/tcl/setup_bootloader_project.tcl"
#   COMMAND "quartus_sh" 
#     "--flow" "compile" "de1soc_bootloader"
#   WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/fpga/de1-soc/fp-bootloader
#   DEPENDS
#     ${CMAKE_SOURCE_DIR}/sdk/build/bootloader/bootloader.mem
#     ${CMAKE_BINARY_DIR}/fpga/de1-soc/fp-bootloader/tcl/setup_bootloader_project.tcl
#   COMMENT "Building FlexPRET bootloader for DE1-SoC"
#   VERBATIM
# )

# We want to include these commands because we do not want to type them out
# for each FPGA project
# include(${PROJECT_SOURCE_DIR}/fpga/cmake/commands.cmake)
