# WaveToAmiga – WAV -> Amiga-Soundformate (8SVX, AIFF, 16SV, MAUD)  (Qt 6 / qmake6)
#
#   mkdir build && cd build
#   qmake6 ../WaveToAmiga.pro CONFIG+=release
#   make            (Windows: mingw32-make / nmake / jom)
#
# Optionen:  CONFIG+=no_tests       Selbsttest nicht mitbauen
#            CONFIG+=no_deploy      (Windows) windeployqt-Schritt nach dem Linken auslassen
#            CONFIG+=no_installer   (Windows) keinen Installer erzeugen

TEMPLATE = subdirs
include(common.pri)

SUBDIRS += soxlib src
soxlib.subdir = soxlib
src.subdir = src
src.depends = soxlib

!no_tests {
    SUBDIRS += tests
    tests.subdir = tests
    tests.depends = soxlib
}

OTHER_FILES += README.md LICENSE THIRD_PARTY_LICENSES.md
