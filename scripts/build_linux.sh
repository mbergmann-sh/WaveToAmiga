#!/usr/bin/env bash
# Baut WaveToAmiga mit qmake6, fuehrt den Selbsttest aus und erzeugt auf Wunsch
# das Deploy-Paket sowie den Qt-Installer-Framework-Installer.
#
#   scripts/build_linux.sh                 nur bauen + Selbsttest
#   scripts/build_linux.sh --deploy        zusaetzlich dist/WaveToAmiga (Programm + Qt-Libs)
#   scripts/build_linux.sh --installer     zusaetzlich Installer (dist/WaveToAmiga-<ver>-linux-setup.run)
#
# Umgebungsvariablen: QMAKE (Standard qmake6), IFW_BIN (Ordner mit binarycreator)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QMAKE="${QMAKE:-qmake6}"; command -v "$QMAKE" >/dev/null 2>&1 || QMAKE=qmake
VERSION="1.0.0"
BUILD="$ROOT/build/release"
MODE="${1:-}"

command -v "$QMAKE" >/dev/null 2>&1 || { echo "qmake6 nicht gefunden (Qt 6: sudo apt install qmake6 qt6-base-dev qt6-multimedia-dev)" >&2; exit 1; }
echo "qmake: $("$QMAKE" -query QMAKE_VERSION | head -1) / Qt $("$QMAKE" -query QT_VERSION)"

mkdir -p "$BUILD"
cd "$BUILD"
"$QMAKE" "$ROOT/WaveToAmiga.pro" CONFIG+=release
make -j"$(nproc)"

echo; echo "=== Selbsttest (WAV -> 8SVX, AIFF, 16SV, MAUD mit libsox) ==="
./bin/test_converter

if [ "$MODE" = "--deploy" ] || [ "$MODE" = "--installer" ]; then
    echo; echo "=== Deploy ==="
    "$ROOT/scripts/deploy_linux.sh"
fi

if [ "$MODE" = "--installer" ]; then
    [ -n "${IFW_BIN:-}" ] && export PATH="$IFW_BIN:$PATH"
    command -v binarycreator >/dev/null 2>&1 || { echo "binarycreator nicht gefunden (Qt Installer Framework; IFW_BIN setzen)" >&2; exit 1; }
    DATA="$ROOT/installer/packages/de.wavetoamiga.app/data"
    mkdir -p "$DATA"
    find "$DATA" -mindepth 1 ! -name .gitkeep -delete
    cp -a "$ROOT/dist/WaveToAmiga/." "$DATA/"      # nur Programm + Bibliotheken
    echo; echo "=== binarycreator ==="
    binarycreator --offline-only -c "$ROOT/installer/config/config_linux.xml" -p "$ROOT/installer/packages" \
        "$ROOT/dist/WaveToAmiga-$VERSION-linux-setup.run"
    echo "Installer: $ROOT/dist/WaveToAmiga-$VERSION-linux-setup.run"
fi
