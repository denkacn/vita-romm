#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

SRC="$SCRIPT_DIR/romm-vita-skeleton"
DST=/home/denkacn/vita-projects/romm-vita-skeleton
OUT="$PROJECT_ROOT/outputs"
LOG="$SCRIPT_DIR/romm-vita-build-inner.log"

exec > >(tee "$LOG") 2>&1

rm -rf "$DST"
mkdir -p "$(dirname "$DST")"
cp -a "$SRC" "$DST"
rm -rf "$DST/build"
chown -R denkacn:denkacn "$DST"

set +e
su - denkacn -c "cd '$DST' && export VITASDK=/usr/local/vitasdk && export PATH=/usr/local/vitasdk/bin:\$PATH && bash scripts/build.sh"
STATUS=$?
set -e
grep -n "vita-make-fself" "$DST/build/CMakeFiles/eboot.bin-self.dir/build.make" || true
echo "BUILD_STATUS=$STATUS"
if [ "$STATUS" -ne 0 ]; then
  exit "$STATUS"
fi

mkdir -p "$OUT"
cp "$DST/build/vita_romm.vpk" "$OUT/vita_romm.vpk"
cp "$DST/build/eboot.bin" "$OUT/vita_romm_eboot.bin"

echo "VPK=$OUT/vita_romm.vpk"
echo "EBOOT=$OUT/vita_romm_eboot.bin"
