#!/usr/bin/env bash
set -euo pipefail

echo "== libs =="
find /usr/local/vitasdk/arm-vita-eabi/lib -maxdepth 2 \
  \( -iname '*zip*' -o -iname '*miniz*' -o -iname '*unzip*' -o -iname '*zlib*' \) | sort

echo "== headers =="
find /usr/local/vitasdk/arm-vita-eabi/include -maxdepth 4 \
  \( -iname '*zip*.h' -o -iname '*unzip*.h' -o -iname '*miniz*.h' -o -iname 'zlib.h' \) | sort
