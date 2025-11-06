# Dummy CMakeLists.txt for LED hardware test folder
# This allows add_subdirectory(leds) in the parent CMakeLists.txt
# No build steps are needed for the static Verilog LED test.

# Optionally, copy the Top.v to the build directory for consistency
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/rtl/Top.v
  ${CMAKE_BINARY_DIR}/fpga/de1-soc/leds/rtl/Top.v
  COPYONLY
)

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/setup_leds_project.tcl
  ${CMAKE_BINARY_DIR}/fpga/de1-soc/leds/setup_leds_project.tcl
  COPYONLY
)
