# Gemeinsame Einstellungen aller Teilprojekte
CONFIG += c++17

# Nur ein Build-Typ pro Lauf (einfachere Ausgabeverzeichnisse, auch unter Windows)
win32 {
    CONFIG -= debug_and_release debug_and_release_target
}
!CONFIG(debug, debug|release): CONFIG += release
