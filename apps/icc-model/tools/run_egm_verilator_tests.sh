#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_DIR="$(cd "${APP_DIR}/../.." && pwd)"
OUTPUT_DIR="${1:-${APP_DIR}/generated/egm_relative/verilator}"
BUILD_DIR="${APP_DIR}/build-verilator-egm-relative"
RESUME_MODE="${2:-}"

if [[ "${RESUME_MODE}" == "--resume" ]]; then
    mkdir -p "${OUTPUT_DIR}"
else
    if [[ -z "${OUTPUT_DIR}" || "${OUTPUT_DIR}" == "/" ]]; then
        echo "refusing to replace unsafe output directory" >&2
        exit 1
    fi
    rm -rf "${OUTPUT_DIR}"
    mkdir -p "${OUTPUT_DIR}"
fi
source "${REPO_DIR}/env.bash"
export RISCV_TOOL_PATH_PREFIX="${RISCV_TOOL_PATH_PREFIX:-/opt/xpack-riscv-none-elf-gcc-14.2.0-2}"

summary="${OUTPUT_DIR}/verilator_summary.csv"
if [[ ! -f "${summary}" ]]; then
    printf '%s\n' \
        'timestep_ms,electrode_cell,electrode_x_um,scenario,direction,q1_time_ms,egm_minimum_time_ms,time_offset_ms,minimum_egm_scaled,min_period_ns,max_period_ns,max_release_lateness_ns,max_execution_time_ns,biological_deadline_ns,deadline_margin_ns' \
        > "${summary}"
fi

expected_offset()
{
    local timestep_ms="$1"
    case "${timestep_ms}" in
        200) echo 200 ;;
        100|50|20) echo 100 ;;
        10) echo 110 ;;
    esac
}

for timestep_ms in 200 100 50 20 10
do
    samples=$(((4500 + timestep_ms - 1) / timestep_ms))
    end_time_ms=$((samples * timestep_ms))
    for electrode_cell in 0 1 2 3 4
    do
        if grep -q "^${timestep_ms},$((electrode_cell + 1))," "${summary}"; then
            continue
        fi
        electrode_x_um=$((electrode_cell * 6000))
        if [[ "${electrode_cell}" -eq 4 ]]; then
            scenario=13
            direction=B_TO_A
        else
            scenario=12
            direction=A_TO_B
        fi
        prefix="${OUTPUT_DIR}/${timestep_ms}ms_cell$((electrode_cell + 1))"

        cmake -S "${APP_DIR}" -B "${BUILD_DIR}" \
            -DTARGET=emulator \
            -DICC_MODEL_TIMESTEP_MS="${timestep_ms}" \
            -DICC_EGM_ELECTRODE_X_UM="${electrode_x_um}" \
            -DICC_VERILATOR_TEST_SCENARIO="${scenario}" \
            -DICC_VERILATOR_TEST_PATH_DELAY_MS=1000 \
            -DICC_VERILATOR_TEST_SAMPLES="${samples}" \
            -DICC_VERILATOR_TEST_PERIOD_NS=1000000 \
            -DICC_VERILATOR_EGM_TRACE=ON \
            > "${prefix}_configure.log"
        cmake --build "${BUILD_DIR}" --target icc-model \
            > "${prefix}_build.log"
        fp-emu +ispm="${BUILD_DIR}/icc-model.mem" > "${prefix}_trace.csv"

        grep -q "^DONE,${samples},${end_time_ms}$" "${prefix}_trace.csv"
        for propagated_cell in 0 1 2 3 4
        do
            if [[ "${scenario}" -eq 12 ]]; then
                expected_q1=$((propagated_cell * 1000))
            else
                expected_q1=$(((4 - propagated_cell) * 1000))
            fi
            actual_q1="$(awk -F, -v cell="${propagated_cell}" \
                '$1 == "Q1" && $4 == cell {print $3; exit}' \
                "${prefix}_trace.csv")"
            test "${actual_q1}" = "${expected_q1}"
        done

        q1_time_ms="$(awk -F, -v cell="${electrode_cell}" \
            '$1 == "Q1" && $4 == cell {print $3; exit}' \
            "${prefix}_trace.csv")"
        read -r minimum_time_ms minimum_value < <(
            awk -F, -v start="${q1_time_ms}" \
                '$1 == "EGM" && $3 >= start && $3 < start + 1000 {
                    if (!found || $5 < minimum) {
                        found = 1; minimum = $5; time = $3
                    }
                }
                END {if (found) print time, minimum}' \
                "${prefix}_trace.csv")
        offset_ms=$((minimum_time_ms - q1_time_ms))
        test "${offset_ms}" -eq "$(expected_offset "${timestep_ms}" "${electrode_cell}")"
        test "${minimum_value}" -lt 0

        cmake -S "${APP_DIR}" -B "${BUILD_DIR}" \
            -DICC_VERILATOR_EGM_TRACE=OFF > "${prefix}_timing_configure.log"
        cmake --build "${BUILD_DIR}" --target icc-model \
            > "${prefix}_timing_build.log"
        fp-emu +ispm="${BUILD_DIR}/icc-model.mem" > "${prefix}_timing.csv"
        grep -q "^DONE,${samples},${end_time_ms}$" "${prefix}_timing.csv"
        timing="$(awk -F, '$1 == "TIMING" {print $2 "," $3 "," $4 "," $5}' \
            "${prefix}_timing.csv")"
        IFS=, read -r min_period max_period max_lateness max_execution <<<"${timing}"
        deadline_ns=$((timestep_ms * 1000000))
        margin_ns=$((deadline_ns - max_execution))
        test "${margin_ns}" -gt 0

        printf '%s\n' \
            "${timestep_ms},$((electrode_cell + 1)),${electrode_x_um},${scenario},${direction},${q1_time_ms},${minimum_time_ms},${offset_ms},${minimum_value},${min_period},${max_period},${max_lateness},${max_execution},${deadline_ns},${margin_ns}" \
            >> "${summary}"
    done
done

echo "Verilator relative-EGM matrix passed: ${summary}"
