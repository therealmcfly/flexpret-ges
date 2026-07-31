cmake_minimum_required(VERSION 3.22)

include($ENV{FP_SDK_PATH}/cmake/riscv-toolchain.cmake)

project(icc-model
  LANGUAGES C
  DESCRIPTION "Single-cell fixed-point ICC model for FlexPRET"
  VERSION 0.1.0
)

set(DEBUG true)

# Keep FPGA as the default, while allowing a separate emulator build with
# -DTARGET=emulator. A cache variable is used so a command-line selection is
# not overwritten when CMake configures the project.
set(TARGET "fpga" CACHE STRING "FlexPRET execution target (fpga or emulator)")
set_property(CACHE TARGET PROPERTY STRINGS fpga emulator)

if (NOT "${TARGET}" STREQUAL "fpga" AND NOT "${TARGET}" STREQUAL "emulator")
  message(FATAL_ERROR "TARGET must be either 'fpga' or 'emulator'")
endif()

file(GLOB APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_executable(icc-model ${APP_SOURCES})

target_include_directories(icc-model PRIVATE
  ${FP_SDK_PATH}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

include($ENV{FP_SDK_PATH}/cmake/fp-app.cmake)
add_subdirectory($ENV{FP_SDK_PATH} BINARY_DIR)
target_link_libraries(icc-model fp-sdk)

if ("${TARGET}" STREQUAL "emulator")
  # The SDK normally generates bin/icc-model for every target. Use a distinct
  # emulator launcher so configuring an emulator build does not overwrite the
  # FPGA flashing launcher.
  set(IMEM_LOCATION ${CMAKE_CURRENT_BINARY_DIR})
  set(IMEM_NAME icc-model.mem)
  configure_file(
    $ENV{FP_SDK_PATH}/cmake/infiles/emu-app.sh.in
    ${CMAKE_SOURCE_DIR}/bin/icc-model-emu
    FILE_PERMISSIONS
      OWNER_EXECUTE OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
    @ONLY
  )

  fp_add_dump_output(icc-model)
  fp_add_mem_output(icc-model)
  fp_add_map_output(icc-model)
  fp_print_memory_usage(icc-model)
  target_link_options(icc-model PRIVATE
    "-Wl,-L" "$ENV{FP_SDK_PATH}/lib/linker/bootloader/none"
  )
else()
  fp_add_outputs(icc-model)
endif()
