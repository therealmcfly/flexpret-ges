#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_DIR="$(cd "${APP_DIR}/../.." && pwd)"
OUTPUT_DIR="${1:-${APP_DIR}/generated/egm_relative/waveforms_natural_a_to_b}"
BUILD_DIR="${APP_DIR}/build-verilator-egm-waveforms"

if [[ -z "${OUTPUT_DIR}" || "${OUTPUT_DIR}" == "/" ]]; then
    echo "refusing to replace unsafe output directory" >&2
    exit 1
fi

rm -rf "${OUTPUT_DIR}"
mkdir -p "${OUTPUT_DIR}"

source "${REPO_DIR}/env.bash"
export RISCV_TOOL_PATH_PREFIX="${RISCV_TOOL_PATH_PREFIX:-/opt/xpack-riscv-none-elf-gcc-14.2.0-2}"

# Scenario 0 gives every cell its natural intrinsic frequency. The initial WAIT
# release makes all cells enter Q1 together near 5 s. That startup transient is
# retained in the raw traces but excluded from the waveform window. The next
# Cell 1 activation is intrinsic and initiates the clean propagated cycle.
for timestep_ms in 200 100 50 20 10
do
    run_end_ms=30000
    samples=$(((run_end_ms + timestep_ms - 1) / timestep_ms))
    end_time_ms=$((samples * timestep_ms))

    for electrode_cell in 0 1 2 3 4
    do
        electrode_x_um=$((electrode_cell * 6000))
        prefix="${OUTPUT_DIR}/${timestep_ms}ms_cell$((electrode_cell + 1))"

        cmake -S "${APP_DIR}" -B "${BUILD_DIR}" \
            -DTARGET=emulator \
            -DICC_MODEL_TIMESTEP_MS="${timestep_ms}" \
            -DICC_EGM_ELECTRODE_X_UM="${electrode_x_um}" \
            -DICC_VERILATOR_TEST_SCENARIO=0 \
            -DICC_VERILATOR_TEST_PATH_DELAY_MS=1000 \
            -DICC_VERILATOR_TEST_SAMPLES="${samples}" \
            -DICC_VERILATOR_TEST_PERIOD_NS=1000000 \
            -DICC_VERILATOR_EGM_TRACE=ON \
            > "${prefix}_configure.log"
        cmake --build "${BUILD_DIR}" --target icc-model \
            > "${prefix}_build.log"
        fp-emu +ispm="${BUILD_DIR}/icc-model.mem" \
            > "${prefix}_trace.csv"

        grep -q "^TEST,0,.*electrode_x_um,${electrode_x_um}$" \
            "${prefix}_trace.csv"
        grep -q "^DONE,${samples},${end_time_ms}$" "${prefix}_trace.csv"
    done

    reference_trace="${OUTPUT_DIR}/${timestep_ms}ms_cell1_trace.csv"
    source_q1_ms="$(awk -F, \
        '$1 == "Q1" && $4 == 0 && $5 == "intrinsic" && $3 > 20000 {print $3; exit}' \
        "${reference_trace}")"
    test -n "${source_q1_ms}"

    for propagated_cell in 1 2 3 4
    do
        expected_q1_ms=$((source_q1_ms + propagated_cell * 1000))
        actual_q1_record="$(awk -F, -v cell="${propagated_cell}" \
            -v start="${source_q1_ms}" \
            '$1 == "Q1" && $4 == cell && $3 >= start {print $3 "," $5; exit}' \
            "${reference_trace}")"
        test "${actual_q1_record}" = "${expected_q1_ms},path"
    done

    window_start_ms=$((source_q1_ms - 1000))
    window_end_ms=$((source_q1_ms + 4500))

    {
        printf '%s\n' \
            'sample,time_ms,time_from_pacemaker_q1_ms,cell_1_egm_scaled,cell_2_egm_scaled,cell_3_egm_scaled,cell_4_egm_scaled,cell_5_egm_scaled'
        paste -d, \
            <(awk -F, -v start="${window_start_ms}" -v end="${window_end_ms}" \
                -v q1="${source_q1_ms}" \
                '$1 == "EGM" && $3 >= start && $3 <= end {print $2 "," $3 "," ($3 - q1) "," $5}' \
                "${OUTPUT_DIR}/${timestep_ms}ms_cell1_trace.csv") \
            <(awk -F, -v start="${window_start_ms}" -v end="${window_end_ms}" \
                '$1 == "EGM" && $3 >= start && $3 <= end {print $5}' \
                "${OUTPUT_DIR}/${timestep_ms}ms_cell2_trace.csv") \
            <(awk -F, -v start="${window_start_ms}" -v end="${window_end_ms}" \
                '$1 == "EGM" && $3 >= start && $3 <= end {print $5}' \
                "${OUTPUT_DIR}/${timestep_ms}ms_cell3_trace.csv") \
            <(awk -F, -v start="${window_start_ms}" -v end="${window_end_ms}" \
                '$1 == "EGM" && $3 >= start && $3 <= end {print $5}' \
                "${OUTPUT_DIR}/${timestep_ms}ms_cell4_trace.csv") \
            <(awk -F, -v start="${window_start_ms}" -v end="${window_end_ms}" \
                '$1 == "EGM" && $3 >= start && $3 <= end {print $5}' \
                "${OUTPUT_DIR}/${timestep_ms}ms_cell5_trace.csv")
    } > "${OUTPUT_DIR}/egm_waveforms_${timestep_ms}ms.csv"

    {
        printf '%s\n' 'cell,q1_time_ms,time_from_pacemaker_q1_ms,cause'
        awk -F, -v start="${source_q1_ms}" -v end="${window_end_ms}" \
            '$1 == "Q1" && $3 >= start && $3 <= end {print ($4 + 1) "," $3 "," ($3 - start) "," $5}' \
            "${reference_trace}"
    } > "${OUTPUT_DIR}/egm_q1_events_${timestep_ms}ms.csv"

    python3 "${SCRIPT_DIR}/render_egm_waveforms.py" \
        "${OUTPUT_DIR}/egm_waveforms_${timestep_ms}ms.csv" \
        "${OUTPUT_DIR}/egm_q1_events_${timestep_ms}ms.csv" \
        "${OUTPUT_DIR}/egm_waveforms_${timestep_ms}ms.svg" \
        "${timestep_ms}"
done

echo "Natural all-timestep five-electrode Verilator waveforms passed: ${OUTPUT_DIR}"
