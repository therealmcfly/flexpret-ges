cmake_minimum_required(VERSION 3.22)

include($ENV{FP_SDK_PATH}/cmake/riscv-toolchain.cmake)

project(icc-model
  LANGUAGES C
  DESCRIPTION "Integer ICC cell and path model for FlexPRET"
  VERSION 0.1.0
)

set(DEBUG true)

# Keep FPGA as the default, while allowing a separate emulator build with
# -DTARGET=emulator. A cache variable is used so a command-line selection is
# not overwritten when CMake configures the project.
set(TARGET "fpga" CACHE STRING "FlexPRET execution target (fpga or emulator)")
set_property(CACHE TARGET PROPERTY STRINGS fpga emulator)

set(ICC_VERILATOR_TEST_SCENARIO "" CACHE STRING
  "Optional emulator-only ICC scenario number")
set(ICC_VERILATOR_TEST_PATH_DELAY_MS "1000" CACHE STRING
  "Uniform path delay used by the emulator scenario test")
set(ICC_VERILATOR_TEST_SAMPLES "1000" CACHE STRING
  "Number of 200 ms model steps in the emulator scenario test")
set(ICC_VERILATOR_TEST_PERIOD_NS "100000" CACHE STRING
  "Accelerated hardware scheduling period for emulator scenario tests")
option(ICC_VERILATOR_EGM_TRACE
  "Print one integer EGM sample per emulator test step" OFF)
set(ICC_MODEL_TIMESTEP_MS "200" CACHE STRING
  "Biological ICC model timestep in milliseconds")
set_property(CACHE ICC_MODEL_TIMESTEP_MS PROPERTY STRINGS 200 100 50 20 10)
set(ICC_MODEL_SUPPORTED_TIMESTEPS 200 100 50 20 10)
if (NOT ICC_MODEL_TIMESTEP_MS IN_LIST ICC_MODEL_SUPPORTED_TIMESTEPS)
  message(FATAL_ERROR
    "ICC_MODEL_TIMESTEP_MS must be one of: 200, 100, 50, 20, 10")
endif()

set(ICC_MODEL_SUPPORTED_INTERVALS -1 0 15 20 23 26 30 40)
foreach(cell_number RANGE 1 5)
  set(interval_option "ICC_CELL${cell_number}_INTERVAL_S")
  if (cell_number EQUAL 1)
    set(interval_default "20")
  else()
    set(interval_default "0")
  endif()
  set(${interval_option} "${interval_default}" CACHE STRING
    "Cell ${cell_number} intrinsic activation interval in seconds")
  set_property(CACHE ${interval_option} PROPERTY STRINGS
    -1 0 15 20 23 26 30 40)
  if (NOT "${${interval_option}}" IN_LIST ICC_MODEL_SUPPORTED_INTERVALS)
    message(FATAL_ERROR
      "${interval_option} must be one of: -1, 0, 15, 20, 23, 26, 30, 40")
  endif()
endforeach()

foreach(path_number RANGE 1 4)
  set(delay_option "ICC_PATH${path_number}_DELAY_MS")
  set(gap_option "ICC_PATH${path_number}_GAP_MM")
  set(${delay_option} "1000" CACHE STRING
    "Path ${path_number} propagation delay in milliseconds")
  set(${gap_option} "6" CACHE STRING
    "Path ${path_number} gap in millimetres")
  if (NOT "${${delay_option}}" STREQUAL "1000")
    message(FATAL_ERROR
      "${delay_option} must be 1000 for the current EGM LUT")
  endif()
  if (NOT "${${gap_option}}" STREQUAL "6")
    message(FATAL_ERROR
      "${gap_option} must be 6 for the current EGM LUT")
  endif()
endforeach()

set(ICC_MODEL_UART "2" CACHE STRING
  "UART used for EGM transmission and pacing reception (1 or 2)")
set_property(CACHE ICC_MODEL_UART PROPERTY STRINGS 1 2)
if (NOT ICC_MODEL_UART STREQUAL "1" AND NOT ICC_MODEL_UART STREQUAL "2")
  message(FATAL_ERROR "ICC_MODEL_UART must be 1 or 2; UART0 is reserved")
endif()

set(ICC_EGM_ELECTRODE_X_UM "6000" CACHE STRING
  "Initial EGM electrode x-position in micrometres")
set(ICC_EGM_SUPPORTED_ELECTRODE_X_UM 0 6000 12000 18000 24000)
if (NOT ICC_EGM_ELECTRODE_X_UM IN_LIST ICC_EGM_SUPPORTED_ELECTRODE_X_UM)
  message(FATAL_ERROR
    "ICC_EGM_ELECTRODE_X_UM must be one of: 0, 6000, 12000, 18000, 24000")
endif()

if (NOT "${TARGET}" STREQUAL "fpga" AND NOT "${TARGET}" STREQUAL "emulator")
  message(FATAL_ERROR "TARGET must be either 'fpga' or 'emulator'")
endif()

set(EGM_GENERATED_DIR
  "${CMAKE_CURRENT_SOURCE_DIR}/generated/egm_relative")
set(EGM_SELECTED_HEADER
  "${EGM_GENERATED_DIR}/egm_relative_lut.h")
if (NOT EXISTS "${EGM_SELECTED_HEADER}")
  message(FATAL_ERROR
    "Missing generated EGM table: ${EGM_SELECTED_HEADER}\n"
    "Run this before configuring the application:\n"
    "  ./tools/generate_egm_luts.sh")
endif()

set(ICC_MODEL_COMMON_SOURCES
  src/app.c
  src/egm.c
  src/icc.c
  src/network.c
  src/path.c
)

# Keep production, interactive-emulator, and finite validation entry points
# independent. Test scenarios never enter the FPGA translation unit.
if ("${TARGET}" STREQUAL "fpga")
  set(ICC_MODEL_ENTRY_POINT src/main.c)
elseif ("${ICC_VERILATOR_TEST_SCENARIO}" STREQUAL "")
  set(ICC_MODEL_ENTRY_POINT src/emulator_main.c)
else()
  set(ICC_MODEL_ENTRY_POINT src/verilator_test_main.c)
endif()

add_executable(icc-model
  ${ICC_MODEL_ENTRY_POINT}
  ${ICC_MODEL_COMMON_SOURCES}
)

target_compile_definitions(icc-model PRIVATE
  ICC_TIMESTEP_MS=${ICC_MODEL_TIMESTEP_MS}U
  ICC_EGM_INITIAL_ELECTRODE_X_UM=${ICC_EGM_ELECTRODE_X_UM}
  ICC_CELL1_INTERVAL_S=${ICC_CELL1_INTERVAL_S}
  ICC_CELL2_INTERVAL_S=${ICC_CELL2_INTERVAL_S}
  ICC_CELL3_INTERVAL_S=${ICC_CELL3_INTERVAL_S}
  ICC_CELL4_INTERVAL_S=${ICC_CELL4_INTERVAL_S}
  ICC_CELL5_INTERVAL_S=${ICC_CELL5_INTERVAL_S}
  ICC_PATH1_DELAY_MS=${ICC_PATH1_DELAY_MS}U
  ICC_PATH2_DELAY_MS=${ICC_PATH2_DELAY_MS}U
  ICC_PATH3_DELAY_MS=${ICC_PATH3_DELAY_MS}U
  ICC_PATH4_DELAY_MS=${ICC_PATH4_DELAY_MS}U
  ICC_PATH1_GAP_MM=${ICC_PATH1_GAP_MM}U
  ICC_PATH2_GAP_MM=${ICC_PATH2_GAP_MM}U
  ICC_PATH3_GAP_MM=${ICC_PATH3_GAP_MM}U
  ICC_PATH4_GAP_MM=${ICC_PATH4_GAP_MM}U
  ICC_MODEL_UART=${ICC_MODEL_UART}
)

target_include_directories(icc-model PRIVATE
  ${FP_SDK_PATH}/lib/include
  ${CMAKE_CURRENT_SOURCE_DIR}/inc
  ${EGM_GENERATED_DIR}
)

include($ENV{FP_SDK_PATH}/cmake/fp-app.cmake)
add_subdirectory($ENV{FP_SDK_PATH} BINARY_DIR)
target_link_libraries(icc-model fp-sdk)

if ("${TARGET}" STREQUAL "emulator")
  if (NOT "${ICC_VERILATOR_TEST_SCENARIO}" STREQUAL "")
    target_compile_definitions(icc-model PRIVATE
      ICC_VERILATOR_TEST_SCENARIO=${ICC_VERILATOR_TEST_SCENARIO}
      ICC_VERILATOR_TEST_PATH_DELAY_MS=${ICC_VERILATOR_TEST_PATH_DELAY_MS}U
      ICC_VERILATOR_TEST_SAMPLES=${ICC_VERILATOR_TEST_SAMPLES}U
      ICC_PERIOD_NS=${ICC_VERILATOR_TEST_PERIOD_NS}U
    )
  endif()

  if (ICC_VERILATOR_EGM_TRACE)
    target_compile_definitions(icc-model PRIVATE ICC_VERILATOR_EGM_TRACE=1)
  endif()

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
