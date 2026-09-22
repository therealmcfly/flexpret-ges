#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_DIR="$(cd "${APP_DIR}/../.." && pwd)"
OUTPUT_DIR="${1:-${APP_DIR}/generated/egm_relative/fpga}"
TOOL_PREFIX="${RISCV_TOOL_PATH_PREFIX:-/opt/xpack-riscv-none-elf-gcc-14.2.0-2}/bin/riscv-none-elf-"
ISPM_CAPACITY=65536
DSPM_CAPACITY=65536
STACK_RESERVATION=2048

mkdir -p "${OUTPUT_DIR}"
source "${REPO_DIR}/env.bash"
export RISCV_TOOL_PATH_PREFIX="${RISCV_TOOL_PATH_PREFIX:-/opt/xpack-riscv-none-elf-gcc-14.2.0-2}"

summary="${OUTPUT_DIR}/fpga_build_summary.csv"
printf '%s\n' \
    'mode,timestep_ms,text_bytes,data_bytes,bss_bytes,ispm_used_bytes,dspm_static_used_bytes,stack_reserved_bytes,total_spm_used_or_reserved_bytes,total_spm_capacity_bytes,total_spm_remaining_bytes,lut_symbol_bytes,lut_symbol_count' \
    > "${summary}"

for mode in single all
do
    for timestep_ms in 200 100 50 20 10
    do
        build_dir="${APP_DIR}/build-fpga-relative-${mode}-${timestep_ms}ms"
        prefix="${OUTPUT_DIR}/${mode}_${timestep_ms}ms"
        cmake -S "${APP_DIR}" -B "${build_dir}" \
            -DTARGET=fpga \
            -DICC_MODEL_TIMESTEP_MS="${timestep_ms}" \
            -DICC_EGM_ELECTRODE_X_UM=6000 \
            -DICC_EGM_OUTPUT_MODE="${mode}" \
            > "${prefix}_configure.log"
        cmake --build "${build_dir}" --target icc-model \
            > "${prefix}_build.log"

        elf="${build_dir}/icc-model"
        egm_object="${build_dir}/CMakeFiles/icc-model.dir/src/egm.c.obj"
        bank_object="${build_dir}/CMakeFiles/icc-model.dir/src/egm_bank.c.obj"
        section_sizes="$("${TOOL_PREFIX}size" -A "${elf}")"
        text_bytes="$(awk '$1 == ".text" {print $2}' <<<"${section_sizes}")"
        data_bytes="$(awk '$1 == ".data" {print $2}' <<<"${section_sizes}")"
        bss_bytes="$(awk '$1 == ".bss" {print $2}' <<<"${section_sizes}")"
        ispm_used=$((text_bytes + data_bytes))
        dspm_used=$((data_bytes + bss_bytes))
        total_used=$((ispm_used + dspm_used + STACK_RESERVATION))
        total_capacity=$((ISPM_CAPACITY + DSPM_CAPACITY))
        remaining=$((total_capacity - total_used))
        lut_symbols="$("${TOOL_PREFIX}nm" -S --size-sort "${elf}" |
            awk '$4 == "kEgmRelativePotential" {print $2}')"
        lut_count="$(wc -w <<<"${lut_symbols}")"
        test "${lut_count}" -eq 1
        lut_bytes=$((16#${lut_symbols}))
        test "${lut_bytes}" -eq 3204
        test "${remaining}" -gt 0

        "${TOOL_PREFIX}size" -A "${elf}" > "${prefix}_sections.txt"
        "${TOOL_PREFIX}nm" -n "${elf}" > "${prefix}_symbols.txt"
        "${TOOL_PREFIX}objdump" -d "${elf}" > "${prefix}_disassembly.txt"
        "${TOOL_PREFIX}objdump" -d "${egm_object}" \
            > "${prefix}_egm_disassembly.txt"
        "${TOOL_PREFIX}objdump" -d "${bank_object}" \
            > "${prefix}_egm_bank_disassembly.txt"
        "${TOOL_PREFIX}nm" -u "${egm_object}" "${bank_object}" \
            > "${prefix}_egm_undefined.txt"

        if grep -Eiq '[[:space:]](f(add|sub|mul|div|sqrt|cvt|mv|lw|sw)|div|divu|rem|remu)[.[:space:]]' \
            "${prefix}_egm_disassembly.txt" "${prefix}_egm_bank_disassembly.txt"; then
            echo "forbidden EGM runtime instruction in ${mode} mode at ${timestep_ms} ms" >&2
            exit 1
        fi
        if grep -Eiq '__.*(div|mod|muldi|divdi|moddi|float|fix|sqrt)' \
            "${prefix}_egm_undefined.txt"; then
            echo "forbidden EGM runtime helper in ${mode} mode at ${timestep_ms} ms" >&2
            exit 1
        fi

        grep -Ei '[[:space:]](f(add|sub|mul|div|sqrt|cvt|mv|lw|sw)|div|divu|rem|remu)[.[:space:]]' \
            "${prefix}_disassembly.txt" > "${prefix}_whole_elf_div_float_hits.txt" || true
        grep -Ei '__.*(div|mod|muldi|divdi|moddi|float|fix|sqrt)' \
            "${prefix}_symbols.txt" > "${prefix}_whole_elf_helper_hits.txt" || true

        printf '%s\n' \
            "${mode},${timestep_ms},${text_bytes},${data_bytes},${bss_bytes},${ispm_used},${dspm_used},${STACK_RESERVATION},${total_used},${total_capacity},${remaining},${lut_bytes},${lut_count}" \
            >> "${summary}"
    done
done

echo "FPGA cross-build and ELF checks passed: ${summary}"
