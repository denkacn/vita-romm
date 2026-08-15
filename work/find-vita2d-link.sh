#!/usr/bin/env bash
set -euo pipefail

grep -R "vita2d" -n /home/denkacn/src /usr/local/vitasdk/share /usr/local/vitasdk/arm-vita-eabi/lib/cmake 2>/dev/null | head -n 120 || true
