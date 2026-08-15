#!/usr/bin/env bash
set -euo pipefail

grep -R "SCE_SYSMODULE_INTERNAL_PROMOTER\|SCE_SYSMODULE_INTERNAL_PAF\|scePromoterUtility" \
  -n /usr/local/vitasdk/arm-vita-eabi/include /usr/local/vitasdk/arm-vita-eabi/lib 2>/dev/null | head -80

echo "== libs =="
find /usr/local/vitasdk/arm-vita-eabi/lib -maxdepth 1 \
  \( -iname '*Promoter*' -o -iname '*Paf*' \) | sort
