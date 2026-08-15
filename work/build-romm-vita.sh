#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/romm-vita-skeleton"
export VITASDK=/usr/local/vitasdk
export PATH=/usr/local/vitasdk/bin:$PATH

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --verbose -j1
