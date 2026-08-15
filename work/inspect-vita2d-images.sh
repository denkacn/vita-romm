#!/usr/bin/env bash
set -euo pipefail
grep -n "load_.*file\|PNG\|JPG\|JPEG" /usr/local/vitasdk/arm-vita-eabi/include/vita2d.h || true
