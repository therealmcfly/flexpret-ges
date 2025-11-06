# CMake build for DE1-SoC fp-rfu (Quartus flow)
set(TOP_LEVEL Top)
set(QSF_FILE ${CMAKE_CURRENT_SOURCE_DIR}/../xdc/DE1_SoC.qsf)
set(VERILOG_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/rtl/Top.v
    # Add other RTL files as needed
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rtl/Top.v
  ${CMAKE_CURRENT_BINARY_DIR}/rtl/Top.v
  COPYONLY
)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/tcl/setup_rfu_project.tcl
  ${CMAKE_CURRENT_BINARY_DIR}/tcl/setup_rfu_project.tcl
  COPYONLY
)

configure_file(
  ${CMAKE_SOURCE_DIR}/apps/rfu/build/rfu.mem
  ${CMAKE_BINARY_DIR}/fpga/de1-soc/fp-rfu/ispm.mem
  COPYONLY
)




#add_custom_target(fpga-synth ALL
#    COMMAND quartus_sh --flow compile ${TOP_LEVEL}
#    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#    COMMENT "Running Quartus synthesis for DE1-SoC fp-blinky"
#    VERBATIM
#)

# Optionally, add a target to generate a .pof or .sof file for programming
# add_custom_target(fpga-program
#     COMMAND quartus_pgm -c USB-Blaster -m JTAG -o "p;output_file.sof"
#     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
#     DEPENDS fpga-synth
#     COMMENT "Programming DE1-SoC with generated SOF file"
#     VERBATIM
# )
