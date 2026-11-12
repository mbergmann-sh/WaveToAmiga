@echo off
rem ===========================================================================
rem  WaveToAmiga - Installer erzeugen (Qt Installer Framework, Windows)
rem
rem  Wird von qmake nach dem Deploy-Schritt aufgerufen (src\src.pro, QMAKE_POST_LINK)
rem  oder von Hand:
rem     make_installer_windows.bat <deployDir> <projectRoot> <qtBinDir> <installerName.exe>
rem
rem  1. binarycreator.exe suchen:  %IFW_DIR%  ->  PATH  ->  <Qt>\Tools\QtInstallerFramework\*\bin
rem  2. Inhalt von <deployDir> (Programm + DLLs + Plugins) in das Installer-Paket kopieren
rem  3. binarycreator -> <projectRoot>\installer\output\<installerName.exe>
rem
rem  Bewusst ohne Klammer-Bloecke (Pfade mit "(x86)" wuerden die Syntax zerstoeren).
rem ===========================================================================
setlocal EnableExtensions

set "DEPLOY=%~1"
set "ROOT=%~2"
set "QTBIN=%~3"
set "NAME=%~4"
set "ERR="
if "%NAME%"=="" goto :usage

if not exist "%DEPLOY%\WaveToAmiga.exe" set "ERR=Deploy-Ordner unvollstaendig: %DEPLOY%\WaveToAmiga.exe fehlt" & goto :fail

rem ---- 0. Inhalt pruefen: nur Programm + Bibliotheken, keine Zwischendateien oder Quelltexte --------
dir /s /b "%DEPLOY%" | findstr /I /R "\.o$ \.obj$ \.a$ \.lib$ \.cpp$ \.c$ \.h$ \.pro$ \.pri$ \.qrc$ \.pdb$ \\moc_ \\qrc_ \\ui_" >nul
if not errorlevel 1 set "ERR=Unerwuenschte Dateien im Deploy-Ordner (Objekt-, moc- oder Quelldateien) - Installer wird nicht gebaut" & goto :fail

rem ---- 1. binarycreator suchen -----------------------------------------------------------------
set "BC="
if defined IFW_DIR if exist "%IFW_DIR%\bin\binarycreator.exe" set "BC=%IFW_DIR%\bin\binarycreator.exe"
if not defined BC if defined IFW_DIR if exist "%IFW_DIR%\binarycreator.exe" set "BC=%IFW_DIR%\binarycreator.exe"
if not defined BC for /f "delims=" %%I in ('where binarycreator.exe 2^>nul') do if not defined BC set "BC=%%I"
if not defined BC for %%I in ("%QTBIN%\..\..\..") do set "QTROOT=%%~fI"
if not defined BC if defined QTROOT for /f "delims=" %%V in ('dir /b /ad /o:n "%QTROOT%\Tools\QtInstallerFramework" 2^>nul') do if exist "%QTROOT%\Tools\QtInstallerFramework\%%V\bin\binarycreator.exe" set "BC=%QTROOT%\Tools\QtInstallerFramework\%%V\bin\binarycreator.exe"
if not defined BC goto :no_ifw
echo [installer] binarycreator: %BC%

rem ---- 2. Installer-Paket befuellen (nur Programm + Bibliotheken aus dem Deploy-Ordner) ---------
set "DATA=%ROOT%\installer\packages\de.wavetoamiga.app\data"
if not exist "%DATA%" mkdir "%DATA%"
for /d %%D in ("%DATA%\*") do rmdir /S /Q "%%D"
for %%F in ("%DATA%\*") do del /F /Q "%%F" >nul 2>&1
xcopy "%DEPLOY%\*" "%DATA%\" /E /I /Y /Q >nul
if errorlevel 1 set "ERR=Kopieren in das Installer-Paket fehlgeschlagen: %DATA%" & goto :fail

rem ---- 3. Installer bauen -----------------------------------------------------------------------
set "OUT=%ROOT%\installer\output"
if not exist "%OUT%" mkdir "%OUT%"
if exist "%OUT%\%NAME%" del /F /Q "%OUT%\%NAME%"
echo [installer] erzeuge %OUT%\%NAME% ...
"%BC%" --offline-only -c "%ROOT%\installer\config\config.xml" -p "%ROOT%\installer\packages" "%OUT%\%NAME%"
if errorlevel 1 set "ERR=binarycreator ist fehlgeschlagen" & goto :fail
if not exist "%OUT%\%NAME%" set "ERR=Installer wurde nicht erzeugt: %OUT%\%NAME%" & goto :fail

rem ---- Paket-Ordner wieder leeren (damit nichts Grosses im Projektbaum liegen bleibt) ---------
for /d %%D in ("%DATA%\*") do rmdir /S /Q "%%D"
for %%F in ("%DATA%\*") do del /F /Q "%%F" >nul 2>&1
type nul > "%DATA%\.gitkeep"

echo [installer] fertig: %OUT%\%NAME%
exit /b 0

:no_ifw
echo [installer] UEBERSPRUNGEN: binarycreator.exe (Qt Installer Framework) nicht gefunden.
echo [installer] Installieren: Qt Maintenance Tool - Qt Installer Framework, danach entweder
echo [installer]   set IFW_DIR=D:\Qt\Tools\QtInstallerFramework\4.x      oder den bin-Ordner in den PATH aufnehmen.
echo [installer] In Qt Creator: Projekte - Build - Build-Umgebung - IFW_DIR hinzufuegen.
exit /b 0

:fail
echo [installer] FEHLER: %ERR%
exit /b 1

:usage
echo Aufruf: make_installer_windows.bat ^<deployDir^> ^<projectRoot^> ^<qtBinDir^> ^<installerName.exe^>
exit /b 2
