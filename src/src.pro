# WaveToAmiga – Qt-Anwendung
include(../common.pri)

TEMPLATE = app
TARGET = WaveToAmiga
QT += widgets multimedia
VERSION = 1.0.0
DEFINES += W2A_VERSION=\\\"$$VERSION\\\" QT_DEPRECATED_WARNINGS
DESTDIR = $$OUT_PWD/../bin

INCLUDEPATH += $$PWD

SOURCES += \
    main.cpp \
    AboutDialog.cpp \
    AppSettings.cpp \
    ConvertManager.cpp \
    FileTable.cpp \
    MainWindow.cpp \
    SettingsDialog.cpp \
    core/Converter.cpp \
    core/Resampler.cpp \
    core/Targets.cpp \
    core/WavReader.cpp

HEADERS += \
    AboutDialog.h \
    AppSettings.h \
    ConvertManager.h \
    FileTable.h \
    MainWindow.h \
    PathUtil.h \
    SettingsDialog.h \
    core/Converter.h \
    core/File.h \
    core/Resampler.h \
    core/Targets.h \
    core/WavReader.h

RESOURCES += $$PWD/../resources/resources.qrc

include(../sox_link.pri)

msvc: QMAKE_CXXFLAGS += /utf-8

# ------------------------------------------------------------------ Windows
win32 {
    # Programm-Icon (mehrere Größen 16–256 px) + Versionsinformationen der .exe
    RC_ICONS = $$PWD/../resources/icons/app.ico
    QMAKE_TARGET_COMPANY = "Michael Bergmann"
    QMAKE_TARGET_PRODUCT = "WaveToAmiga"
    QMAKE_TARGET_DESCRIPTION = "WaveToAmiga - WAV to Amiga sound format converter"
    QMAKE_TARGET_COPYRIGHT = "Copyright (c) 2026 Michael Bergmann (MIT License)"
    RC_LANG = 0x0407

    # ---- Deploy-Schritt nach jedem Link: windeployqt + MinGW-Laufzeit -----------
    # Ergebnis: <build>/deploy  = fertiger Ordner (nur .exe + DLLs + Plugins + Lizenztexte),
    # genau diesen Ordner packt anschliessend der Installer. Abschalten: CONFIG+=no_deploy
    !no_deploy {
        WINDEPLOYQT = $$[QT_INSTALL_BINS]/windeployqt.exe
        !exists($$WINDEPLOYQT): WINDEPLOYQT = $$[QT_INSTALL_BINS]/windeployqt6.exe

        win32-g++*: DEPLOY_COMPILER = mingw
        else:       DEPLOY_COMPILER = msvc

        CONFIG(debug, debug|release): DEPLOY_BUILDTYPE = debug
        else:                         DEPLOY_BUILDTYPE = release

        DEPLOY_EXE  = $$shell_path($$OUT_PWD/../bin/$${TARGET}.exe)
        DEPLOY_DIR  = $$shell_path($$OUT_PWD/../deploy)

        QMAKE_POST_LINK += $$shell_quote($$shell_path($$PWD/../scripts/deploy_windows.bat)) \
            $$shell_quote($$DEPLOY_EXE) \
            $$shell_quote($$DEPLOY_DIR) \
            $$shell_quote($$shell_path($$WINDEPLOYQT)) \
            $$DEPLOY_COMPILER \
            $$shell_quote($$shell_path($$PWD/..)) \
            $$DEPLOY_BUILDTYPE \
            $$escape_expand(\n\t)

        # ---- Installer (Qt Installer Framework) direkt hinter dem Deploy-Schritt -------------
        # Ergebnis:  <Projekt>\installer\output\WaveToAmigaSetup_IFW.exe       (Release)
        #            <Projekt>\installer\output\WaveToAmigaSetup_IFW_debug.exe (Debug)
        # Abschalten: CONFIG+=no_installer   (dauert pro Link ca. 10-60 s, je nach Rechner)
        # binarycreator wird ueber IFW_DIR, den PATH oder <Qt>\Tools\QtInstallerFramework gefunden.
        !no_installer {
            INSTALLER_NAME = WaveToAmigaSetup_IFW.exe
            CONFIG(debug, debug|release): INSTALLER_NAME = WaveToAmigaSetup_IFW_debug.exe

            QMAKE_POST_LINK += $$shell_quote($$shell_path($$PWD/../scripts/make_installer_windows.bat)) \
                $$shell_quote($$DEPLOY_DIR) \
                $$shell_quote($$shell_path($$PWD/..)) \
                $$shell_quote($$shell_path($$[QT_INSTALL_BINS])) \
                $$INSTALLER_NAME \
                $$escape_expand(\n\t)
        }
    }
}

# ------------------------------------------------------------------ Linux
unix:!macx {
    # RPATH relativ zur Programmdatei: gebündelte Qt-Bibliotheken in ../lib werden gefunden
    QMAKE_LFLAGS += "-Wl,-rpath,\'\$$ORIGIN/../lib\'"

    # make install  (PREFIX=/usr  ->  /usr/bin, /usr/share/applications, /usr/share/icons/hicolor)
    isEmpty(PREFIX): PREFIX = /usr/local
    target.path = $$PREFIX/bin
    desktop.path = $$PREFIX/share/applications
    desktop.files = $$PWD/../packaging/linux/WaveToAmiga.desktop
    icons.path = $$PREFIX/share/icons
    icons.files = $$PWD/../packaging/linux/icons/hicolor
    INSTALLS += target desktop icons
}
