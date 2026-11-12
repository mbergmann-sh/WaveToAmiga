# libsox-Teilmenge (SoX 14.4.2-Quellen) als statische Bibliothek 'w2asox'.
#   Original-Handler:  8svx.c, aiff.c/aiff-fmt.c, maud.c
#   Ergänzungen:       svx_ext.c (8SVX-Fibonacci, 16SV), w2a_stubs.c, eigene formats.h/soxconfig.h
#   Änderung:          formats.c (UTF-8-Dateinamen unter Windows, im Quelltext gekennzeichnet)
# Ausgabe: <build>/lib/libw2asox.a  (MSVC: w2asox.lib)
include(../common.pri)

TEMPLATE = lib
TARGET = w2asox
CONFIG += staticlib c99 warn_off
CONFIG -= qt
DESTDIR = $$OUT_PWD/../lib

SOX = $$PWD/../third_party/sox/src

!exists($$SOX/formats.c) {
    error("SoX-Quellen fehlen: $$SOX/formats.c")
}

INCLUDEPATH += $$SOX

win32: DEFINES += WIN32   # SoX prüft das Makro WIN32 (z. B. für lsx_tmpfile)
win32-msvc*: DEFINES += _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS

SOURCES += \
    $$SOX/libsox.c \
    $$SOX/libsox_i.c \
    $$SOX/formats.c \
    $$SOX/formats_i.c \
    $$SOX/util.c \
    $$SOX/xmalloc.c \
    $$SOX/raw.c \
    $$SOX/g711.c \
    $$SOX/id3.c \
    $$SOX/8svx.c \
    $$SOX/aiff.c \
    $$SOX/aiff-fmt.c \
    $$SOX/maud.c \
    $$SOX/svx_ext.c \
    $$SOX/w2a_stubs.c

HEADERS += \
    $$SOX/sox.h $$SOX/sox_i.h $$SOX/soxomp.h $$SOX/soxconfig.h $$SOX/util.h $$SOX/xmalloc.h \
    $$SOX/formats.h $$SOX/effects.h $$SOX/raw.h $$SOX/g711.h $$SOX/aiff.h $$SOX/id3.h
