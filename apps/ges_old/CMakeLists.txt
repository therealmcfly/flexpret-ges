cmake_minimum_required(VERSION 3.22)

# Set toolchain to RISC-V; must be done before call to project()
include($ENV{FP_SDK_PATH}/cmake/riscv-toolchain.cmake)

project(ges_old
  LANGUAGES C
  DESCRIPTION "An application fit for FPGA which blinks LEDs"
  VERSION 1.0.0
)

set(DEBUG true)
set(TARGET "fpga")

# Automatically collect ALL .c files in src/
file(GLOB APP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

# Create the executable with all sources
add_executable(ges_old ${APP_SOURCES})

# Add your include directory
# target_include_directories(ges_old PRIVATE
#     ${CMAKE_CURRENT_SOURCE_DIR}/inc
# )

# Add SDK include directory AND your own include directory
target_include_directories(ges_old PRIVATE
    ${FP_SDK_PATH}/lib/include       # <-- REQUIRED for hwconfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

# FlexPRET integration
include($ENV{FP_SDK_PATH}/cmake/fp-app.cmake)

# Add sdk as subdirectory out-of-tree and link it
add_subdirectory($ENV{FP_SDK_PATH} BINARY_DIR)
target_link_libraries(ges_old fp-sdk)

fp_add_outputs(ges_old)
