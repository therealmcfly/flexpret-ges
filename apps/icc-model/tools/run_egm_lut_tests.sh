#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TEMP_DIR}"' EXIT

gcc -std=c11 -Wall -Wextra -Werror \
    "${SCRIPT_DIR}/generate_egm_lut.c" -lm \
    -o "${TEMP_DIR}/generate-egm-lut"

mkdir -p "${TEMP_DIR}/first" "${TEMP_DIR}/second"
"${TEMP_DIR}/generate-egm-lut" "${TEMP_DIR}/first" >/dev/null
"${TEMP_DIR}/generate-egm-lut" "${TEMP_DIR}/second" >/dev/null
diff -ru "${TEMP_DIR}/first" "${TEMP_DIR}/second"

test "$(wc -l < "${TEMP_DIR}/first/egm_relative_lut.csv")" -eq 802
test "$(find "${TEMP_DIR}/first" -maxdepth 1 -type f | wc -l)" -eq 3

gcc -std=c11 -Wall -Wextra -Werror \
    -I"${TEMP_DIR}/first" \
    "${APP_DIR}/tests/test_egm_lut.c" -lm \
    -o "${TEMP_DIR}/test-egm-lut"
"${TEMP_DIR}/test-egm-lut"

first_hash="$(sha256sum "${TEMP_DIR}/first/egm_relative_lut.h" | awk '{print $1}')"
second_hash="$(sha256sum "${TEMP_DIR}/second/egm_relative_lut.h" | awk '{print $1}')"
file_size="$(wc -c < "${TEMP_DIR}/first/egm_relative_lut.h")"
echo "REPRODUCIBILITY,first_sha256,${first_hash},second_sha256,${second_hash},header_file_bytes,${file_size},entries,801,table_bytes,3204"
echo "EGM relative lookup generator tests passed"
