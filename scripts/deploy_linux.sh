#!/usr/bin/env bash
# Erzeugt dist/WaveToAmiga/ aus dem fertigen Build:
#   bin/WaveToAmiga  lib/*.so*  plugins/*  share/{applications,icons}  qt.conf  LICENSE
# Es werden NUR das Programm, die Qt-Laufzeitbibliotheken und Plugins kopiert -
# keine Objektdateien (.o), keine moc-/qrc-Dateien und kein Quellcode.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QMAKE="${QMAKE:-qmake6}"; command -v "$QMAKE" >/dev/null 2>&1 || QMAKE=qmake
BUILD="${BUILD_DIR:-$ROOT/build/release}"
STAGE="${STAGE_DIR:-$ROOT/dist/WaveToAmiga}"
EXE="$BUILD/bin/WaveToAmiga"

[ -x "$EXE" ] || { echo "Nicht gefunden: $EXE  (zuerst scripts/build_linux.sh ausfuehren)" >&2; exit 1; }
QT_LIBS="$("$QMAKE" -query QT_INSTALL_LIBS)"
QT_PLUGINS="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
echo "Qt-Bibliotheken: $QT_LIBS"
echo "Qt-Plugins:      $QT_PLUGINS"

rm -rf "$STAGE"
mkdir -p "$STAGE"/{bin,lib,plugins,share/applications}

# ---- Programm -------------------------------------------------------------
install -m 755 "$EXE" "$STAGE/bin/WaveToAmiga"
strip --strip-unneeded "$STAGE/bin/WaveToAmiga" 2>/dev/null || true

# ---- Qt-Plugins (nur die, die das Programm braucht) -----------------------
for d in platforms platformthemes platforminputcontexts xcbglintegrations imageformats \
         iconengines multimedia tls networkinformation \
         wayland-decoration-client wayland-graphics-integration-client wayland-shell-integration; do
    [ -d "$QT_PLUGINS/$d" ] && cp -a "$QT_PLUGINS/$d" "$STAGE/plugins/"
done

# ---- Laufzeitbibliotheken per ldd einsammeln (mehrere Durchgaenge fuer Folgeabhaengigkeiten)
export LD_LIBRARY_PATH="$QT_LIBS${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
want_lib() {   # 0 = mitliefern
    local path="$1" base; base="$(basename "$1")"
    case "$path" in "$QT_LIBS"/*) return 0 ;; esac
    case "$base" in
        libQt6*|libicu*|libavcodec*|libavformat*|libavutil*|libavfilter*|libavdevice*|libswresample*|libswscale*) return 0 ;;
        libdouble-conversion*|libb2.so*|libpcre2-16*|libmd4c*) return 0 ;;
    esac
    return 1
}
for pass in 1 2 3; do
    changed=0
    while IFS= read -r elf; do
        while IFS= read -r lib; do
            [ -n "$lib" ] || continue
            if want_lib "$lib" && [ ! -e "$STAGE/lib/$(basename "$lib")" ]; then
                cp -L "$lib" "$STAGE/lib/$(basename "$lib")"; changed=1
            fi
        done < <(ldd "$elf" 2>/dev/null | awk '/=> \// { print $3 }')
    done < <(find "$STAGE/bin" "$STAGE/plugins" "$STAGE/lib" -type f \( -name 'WaveToAmiga' -o -name '*.so*' \))
    [ "$changed" -eq 1 ] || break
done
find "$STAGE/lib" -type f -name '*.so*' -exec strip --strip-unneeded {} + 2>/dev/null || true

# ---- RUNPATH setzen (falls patchelf vorhanden) ----------------------------
if command -v patchelf >/dev/null 2>&1; then
    patchelf --set-rpath '$ORIGIN/../lib' "$STAGE/bin/WaveToAmiga"
    find "$STAGE/lib" -type f -name '*.so*' -exec patchelf --set-rpath '$ORIGIN' {} \; 2>/dev/null || true
    find "$STAGE/plugins" -type f -name '*.so' -exec patchelf --set-rpath '$ORIGIN/../../lib' {} \; 2>/dev/null || true
else
    echo "Hinweis: patchelf nicht installiert - es wird der RPATH des Programms bzw. der offizielle Qt-RUNPATH der Plugins verwendet."
fi

# ---- qt.conf: Plugins/Bibliotheken relativ zum Programm -------------------
cat > "$STAGE/bin/qt.conf" << 'QTCONF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
QTCONF

# ---- Desktop-Integration + Lizenzen ---------------------------------------
cp "$ROOT/packaging/linux/WaveToAmiga.desktop" "$STAGE/share/applications/"
cp -a "$ROOT/packaging/linux/icons" "$STAGE/share/"
cp "$ROOT/LICENSE" "$STAGE/LICENSE.txt"
cp "$ROOT/THIRD_PARTY_LICENSES.md" "$STAGE/THIRD_PARTY_LICENSES.txt"

# ---- Sicherheitspruefung: nichts Unerwuenschtes im Paket ------------------
bad="$(find "$STAGE" -type f \( -name '*.o' -o -name '*.a' -o -name '*.cpp' -o -name '*.c' -o -name '*.h' \
        -o -name '*.pro' -o -name '*.pri' -o -name '*.qrc' -o -name 'moc_*' -o -name 'qrc_*' -o -name 'ui_*' \))"
if [ -n "$bad" ]; then echo "Unerwuenschte Dateien im Paket:" >&2; echo "$bad" >&2; exit 1; fi

echo
echo "Fehlende Abhaengigkeiten (sollte leer sein, ausser System-Bibliotheken):"
LD_LIBRARY_PATH="$STAGE/lib" ldd "$STAGE/bin/WaveToAmiga" | grep "not found" || echo "  keine"
echo
echo "Paket: $STAGE  ($(du -sh "$STAGE" | cut -f1), $(find "$STAGE" -type f | wc -l) Dateien)"
