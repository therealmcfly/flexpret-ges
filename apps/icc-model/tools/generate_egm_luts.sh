#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUTPUT_ROOT="${APP_DIR}/generated"
OUTPUT_DIR="${OUTPUT_ROOT}/egm_1d5c"
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TEMP_DIR}"' EXIT

if [[ "${OUTPUT_DIR}" != "${APP_DIR}/generated/egm_1d5c" ]]; then
    echo "refusing to replace unexpected output directory: ${OUTPUT_DIR}" >&2
    exit 1
fi

gcc -std=c11 -Wall -Wextra -Werror \
    "${SCRIPT_DIR}/generate_egm_lut.c" \
    -lm \
    -o "${TEMP_DIR}/generate-egm-lut"

mkdir -p "${TEMP_DIR}/generated"
"${TEMP_DIR}/generate-egm-lut" "${TEMP_DIR}/generated"

{
    sha256sum "${SCRIPT_DIR}/generate_egm_lut.c" |
        awk '{print $1 "  ../../tools/generate_egm_lut.c"}'
    find "${TEMP_DIR}/generated" -maxdepth 1 -type f \
        ! -name 'GENERATION_SHA256SUMS.txt' -printf '%f\n' |
        sort |
        while IFS= read -r filename; do
            sha256sum "${TEMP_DIR}/generated/${filename}" |
                awk -v name="${filename}" '{print $1 "  " name}'
        done
} > "${TEMP_DIR}/generated/GENERATION_SHA256SUMS.txt"

mkdir -p "${OUTPUT_ROOT}"
rm -rf "${OUTPUT_DIR}"
mv "${TEMP_DIR}/generated" "${OUTPUT_DIR}"

echo "EGM lookup tables and result data generated in ${OUTPUT_DIR}"
