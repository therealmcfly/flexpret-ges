#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TEMP_DIR}"' EXIT

gcc -std=c11 -Wall -Wextra -Werror \
    "${SCRIPT_DIR}/generate_egm_lut.c" \
    -lm \
    -o "${TEMP_DIR}/generate-egm-lut"

mkdir -p "${TEMP_DIR}/generated-first"
mkdir -p "${TEMP_DIR}/generated-second"
"${TEMP_DIR}/generate-egm-lut" "${TEMP_DIR}/generated-first" >/dev/null
"${TEMP_DIR}/generate-egm-lut" "${TEMP_DIR}/generated-second" >/dev/null

diff -ru "${TEMP_DIR}/generated-first" "${TEMP_DIR}/generated-second"

test "$(wc -l < "${TEMP_DIR}/generated-first/egm_five_electrode_waveforms.csv")" \
    -eq 3701
test "$(wc -l < "${TEMP_DIR}/generated-first/egm_five_electrode_summary.csv")" \
    -eq 26
awk -F, '
    NR == 1 { next }
    {
        key = $1 FS $2 FS $5
        if (seen[key]++) {
            exit 1
        }
    }
' "${TEMP_DIR}/generated-first/egm_five_electrode_waveforms.csv"

for specification in \
    "200 5" \
    "100 10" \
    "50 20" \
    "20 50" \
    "10 100"
do
    read -r timestep_ms step_count <<<"${specification}"
    gcc -std=c11 -Wall -Wextra -Werror \
        -I"${TEMP_DIR}/generated-first" \
        -DEGM_LUT_HEADER=\"egm_lut_${timestep_ms}ms.h\" \
        -DEXPECTED_EGM_TIMESTEP_MS=${timestep_ms}U \
        -DEXPECTED_EGM_STEP_COUNT=${step_count}U \
        "${APP_DIR}/tests/test_egm_lut.c" \
        -o "${TEMP_DIR}/test-egm-lut-${timestep_ms}"
    "${TEMP_DIR}/test-egm-lut-${timestep_ms}"
done

echo "EGM lookup generator tests passed"
