#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
APP_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TEMP_DIR="$(mktemp -d)"
trap 'rm -rf "${TEMP_DIR}"' EXIT

for timestep_ms in 200 100 50 20 10
do
    gcc -std=c11 -Wall -Wextra -Werror \
        -DICC_TIMESTEP_MS=${timestep_ms}U \
        -I"${APP_DIR}/inc" \
        -I"${APP_DIR}/generated/egm_relative" \
        "${APP_DIR}/src/egm.c" \
        "${APP_DIR}/src/icc.c" \
        "${APP_DIR}/src/path.c" \
        "${APP_DIR}/src/network.c" \
        "${APP_DIR}/tests/test_egm_runtime.c" \
        -o "${TEMP_DIR}/test-egm-runtime-${timestep_ms}"
    "${TEMP_DIR}/test-egm-runtime-${timestep_ms}"
done

echo "EGM relative runtime tests passed for all supported timesteps"
