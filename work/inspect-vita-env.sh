#!/usr/bin/env bash
set -euo pipefail

export VITASDK=/usr/local/vitasdk
export PATH=/usr/local/vitasdk/bin:$PATH

echo "== tools =="
command -v vitacompanion || true
command -v vitacompanion-cli || true
command -v vita-make-fself || true

echo "== vita2d header snippets =="
grep -n "vita2d_.*pgf\|vita2d_.*font\|vita2d_start_drawing\|vita2d_draw_text" /usr/local/vitasdk/arm-vita-eabi/include/vita2d.h | head -n 80 || true

echo "== curl cmake =="
ls /usr/local/vitasdk/arm-vita-eabi/lib/cmake/CURL || true
