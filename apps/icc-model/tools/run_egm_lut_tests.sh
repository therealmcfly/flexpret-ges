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

mkdir -p "${TEMP_DIR}/generated"
"${TEMP_DIR}/generate-egm-lut" "${TEMP_DIR}/generated" >/dev/null

diff -ru "${APP_DIR}/generated/egm_1path" "${TEMP_DIR}/generated"

for specification in \
    "200 5" \
    "100 10" \
    "50 20" \
    "20 50" \
    "10 100"
do
    read -r timestep_ms step_count <<<"${specification}"
    gcc -std=c11 -Wall -Wextra -Werror \
        -I"${TEMP_DIR}/generated" \
        -DEGM_LUT_HEADER=\"egm_lut_${timestep_ms}ms.h\" \
        -DEXPECTED_EGM_TIMESTEP_MS=${timestep_ms}U \
        -DEXPECTED_EGM_STEP_COUNT=${step_count}U \
        "${APP_DIR}/tests/test_egm_lut.c" \
        -o "${TEMP_DIR}/test-egm-lut-${timestep_ms}"
    "${TEMP_DIR}/test-egm-lut-${timestep_ms}"
done

echo "EGM lookup generator tests passed"
