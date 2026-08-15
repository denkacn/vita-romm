#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"

export VITASDK="${VITASDK:-/usr/local/vitasdk}"
export PATH="${VITASDK}/bin:${PATH}"

cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

echo "Built: ${BUILD_DIR}/vita_romm.vpk"
