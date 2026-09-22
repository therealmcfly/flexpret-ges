#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_DIR="$(cd "${APP_DIR}/../.." && pwd)"
OUTPUT_DIR="${1:-${APP_DIR}/generated/egm_relative/verilator_multi}"
BUILD_DIR="${APP_DIR}/build-verilator-egm-multi"

if [[ -z "${OUTPUT_DIR}" || "${OUTPUT_DIR}" == "/" ]]; then
    echo "refusing to replace unsafe output directory" >&2
    exit 1
fi
rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

source "${REPO_DIR}/env.bash"
export RISCV_TOOL_PATH_PREFIX="${RISCV_TOOL_PATH_PREFIX:-/opt/xpack-riscv-none-elf-gcc-14.2.0-2}"

summary="${OUTPUT_DIR}/verilator_multi_summary.csv"
printf '%s\n' \
    'timestep_ms,scenario,direction,samples,max_execution_time_ns,biological_deadline_ns,deadline_margin_ns' \
    > "${summary}"

for timestep_ms in 200 100 50 20 10
do
    samples=$(((4500 + timestep_ms - 1) / timestep_ms))
    end_time_ms=$((samples * timestep_ms))
    deadline_ns=$((timestep_ms * 1000000))

    for scenario in 12 13
    do
        if [[ "${scenario}" -eq 12 ]]; then
            direction=A_TO_B
        else
            direction=B_TO_A
        fi
        prefix="${OUTPUT_DIR}/${timestep_ms}ms_scenario${scenario}"

        cmake -S "${APP_DIR}" -B "${BUILD_DIR}" \
            -DTARGET=emulator \
            -DICC_MODEL_TIMESTEP_MS="${timestep_ms}" \
            -DICC_EGM_OUTPUT_MODE=all \
            -DICC_EGM_ELECTRODE_X_UM=0 \
            -DICC_VERILATOR_TEST_SCENARIO="${scenario}" \
            -DICC_VERILATOR_TEST_PATH_DELAY_MS=1000 \
            -DICC_VERILATOR_TEST_SAMPLES="${samples}" \
            -DICC_VERILATOR_TEST_PERIOD_NS=1000000 \
            -DICC_VERILATOR_EGM_TRACE=ON \
            > "${prefix}_all_configure.log"
        cmake --build "${BUILD_DIR}" --target icc-model \
            > "${prefix}_all_build.log"
        fp-emu +ispm="${BUILD_DIR}/icc-model.mem" > "${prefix}_all.csv"

        grep -q "^EGM_MODE,all,channels,5$" "${prefix}_all.csv"
        grep -q "^DONE,${samples},${end_time_ms}$" "${prefix}_all.csv"
        for propagated_cell in 0 1 2 3 4
        do
            if [[ "${scenario}" -eq 12 ]]; then
                expected_q1=$((propagated_cell * 1000))
            else
                expected_q1=$(((4 - propagated_cell) * 1000))
            fi
            actual_q1="$(awk -F, -v cell="${propagated_cell}" \
                '$1 == "Q1" && $4 == cell {print $3; exit}' \
                "${prefix}_all.csv")"
            test "${actual_q1}" = "${expected_q1}"
        done

        for channel in 0 1 2 3 4
        do
            electrode_x_um=$((channel * 6000))
            single="${prefix}_cell$((channel + 1))_single.csv"
            cmake -S "${APP_DIR}" -B "${BUILD_DIR}" \
                -DICC_EGM_OUTPUT_MODE=single \
                -DICC_EGM_ELECTRODE_X_UM="${electrode_x_um}" \
                -DICC_VERILATOR_EGM_TRACE=ON \
                > "${prefix}_cell$((channel + 1))_configure.log"
            cmake --build "${BUILD_DIR}" --target icc-model \
                > "${prefix}_cell$((channel + 1))_build.log"
            fp-emu +ispm="${BUILD_DIR}/icc-model.mem" > "${single}"
            grep -q "^EGM_MODE,single,channels,1$" "${single}"
            grep -q "^DONE,${samples},${end_time_ms}$" "${single}"
            awk -F, -v column="$((channel + 4))" '
                NR == FNR && $1 == "EGM_ALL" {expected[$2] = $column; next}
                $1 == "EGM" {
                    if (!($2 in expected) || $5 != expected[$2]) {
                        exit 1
                    }
                    compared++
                }
                END {if (compared == 0) exit 1}
            ' "${prefix}_all.csv" "${single}"
        done

        cmake -S "${APP_DIR}" -B "${BUILD_DIR}" \
            -DICC_EGM_OUTPUT_MODE=all \
            -DICC_EGM_ELECTRODE_X_UM=0 \
            -DICC_VERILATOR_EGM_TRACE=OFF \
            > "${prefix}_timing_configure.log"
        cmake --build "${BUILD_DIR}" --target icc-model \
            > "${prefix}_timing_build.log"
        fp-emu +ispm="${BUILD_DIR}/icc-model.mem" > "${prefix}_timing.csv"
        grep -q "^DONE,${samples},${end_time_ms}$" "${prefix}_timing.csv"
        maximum_execution_time="$(awk -F, '$1 == "TIMING" {print $5}' \
            "${prefix}_timing.csv")"
        margin_ns=$((deadline_ns - maximum_execution_time))
        test "${margin_ns}" -gt 0

        printf '%s\n' \
            "${timestep_ms},${scenario},${direction},${samples},${maximum_execution_time},${deadline_ns},${margin_ns}" \
            >> "${summary}"
    done
done

echo "Verilator simultaneous-EGM matrix passed: ${summary}"
