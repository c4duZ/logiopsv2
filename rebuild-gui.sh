#!/usr/bin/env bash
# Foolproof GUI rebuild + install.
#
# Qt 6.4's incremental qmlcachegen can leave a stale per-file .qmlc for the
# states/ screens after a full `cmake --build build`, producing the runtime
# "XState is not a type" load failure. The reliable fix is to wipe the qmlcache
# and do a targeted rebuild. This script does that, smoke-tests the result, and
# only then installs — so a corrupt binary never reaches /usr/bin.
set -euo pipefail
cd "$(dirname "$0")"

echo "==> Clearing stale qmlcache (build + Qt runtime cache)..."
rm -rf build/src/logiops-gui/.rcc \
       build/src/logiops-gui/CMakeFiles/logiops-gui.dir/.rcc \
       build/src/logiops-gui/logiops \
       "$HOME/.cache/logiops"

echo "==> Building logiops-gui (targeted)..."
cmake --build build --target logiops-gui

echo "==> Smoke test (offscreen QML load)..."
if sg logiops -c "timeout 8 env QT_QPA_PLATFORM=offscreen ./build/src/logiops-gui/logiops-gui 2>&1" \
     | grep -qiE "not a type|Unknown method|failed to load"; then
  echo "!! QML load FAILED — refusing to install a broken binary." >&2
  exit 1
fi
echo "    QML OK."

echo "==> Installing (sudo)..."
sudo cmake --install build

echo "==> Done. The daemon + GUI are installed. Run:  logiops-gui"
