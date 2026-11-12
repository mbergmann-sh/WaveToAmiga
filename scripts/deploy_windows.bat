@echo off
rem ===========================================================================
rem  WaveToAmiga - Deploy-Schritt fuer Windows (Qt 6, MinGW oder MSVC)
rem
rem  Wird von qmake nach dem Linken aufgerufen (QMAKE_POST_LINK in src\src.pro)
rem  und von scripts\build_windows.ps1. Ergebnis: ein Ordner, der alles enthaelt,
rem  was das Programm zum Laufen braucht - und sonst nichts:
rem     WaveToAmiga.exe, Qt6*.dll, plugins\*, MinGW-Laufzeit, Lizenzen
rem
rem  Aufruf:
rem     deploy_windows.bat <exe> <deployDir> <windeployqt.exe> <mingw|msvc> <projectRoot> [release|debug]
rem
rem  Hinweis: Bewusst ohne Klammer-Bloecke geschrieben, damit Pfade wie
rem  "C:\Program Files (x86)\..." die Syntax nicht zerstoeren.
rem ===========================================================================
setlocal EnableExtensions

set "EXE=%~1"
set "DEPLOY=%~2"
set "WDQ=%~3"
set "COMPILER=%~4"
set "ROOT=%~5"
set "BT=%~6"
if "%BT%"=="" set "BT=release"
set "ERR="

if "%ROOT%"=="" goto :usage
if not exist "%EXE%" set "ERR=Programm nicht gefunden: %EXE%" & goto :fail
if not exist "%WDQ%" set "ERR=windeployqt nicht gefunden: %WDQ%" & goto :fail
if defined MINGW_BIN set "PATH=%MINGW_BIN%;%PATH%"

echo [deploy] Ziel: %DEPLOY%

rem ---- 1. Frischer Ordner (keine Altlasten) ------------------------------------------------
if exist "%DEPLOY%" rmdir /S /Q "%DEPLOY%"
mkdir "%DEPLOY%"
if errorlevel 1 set "ERR=Ordner kann nicht angelegt werden: %DEPLOY%" & goto :fail

rem ---- 2. Programm (nur die fertige .exe - keine .o-, moc- oder Quelldateien) ---------------
copy /Y "%EXE%" "%DEPLOY%\" >nul
if errorlevel 1 set "ERR=Programm konnte nicht kopiert werden" & goto :fail

rem ---- 3. windeployqt: Qt6-DLLs, Plugins (platforms, multimedia, ...), FFmpeg ------------------
echo [deploy] windeployqt ...
"%WDQ%" --%BT% --no-translations --no-system-d3d-compiler --no-opengl-sw "%DEPLOY%\WaveToAmiga.exe"
if errorlevel 1 set "ERR=windeployqt ist fehlgeschlagen" & goto :fail

rem ---- 4. MinGW-Laufzeit sicherstellen (falls windeployqt sie nicht gefunden hat) -------------
if /I not "%COMPILER%"=="mingw" goto :runtime_done
set "GXXDIR="
for /f "delims=" %%I in ('where g++.exe 2^>nul') do if not defined GXXDIR set "GXXDIR=%%~dpI"
for %%D in (libstdc++-6.dll libwinpthread-1.dll libgcc_s_seh-1.dll libgcc_s_dw2-1.dll) do call :copy_runtime %%D
if not exist "%DEPLOY%\libstdc++-6.dll" goto :runtime_missing
if not exist "%DEPLOY%\libwinpthread-1.dll" goto :runtime_missing
if exist "%DEPLOY%\libgcc_s_seh-1.dll" goto :runtime_done
if exist "%DEPLOY%\libgcc_s_dw2-1.dll" goto :runtime_done
:runtime_missing
set "ERR=MinGW-Laufzeit-DLLs fehlen. In der Qt-MinGW-Eingabeaufforderung bauen oder MINGW_BIN setzen (z. B. C:\Qt\Tools\mingw1310_64\bin)"
goto :fail
:runtime_done

rem ---- 5. Vollstaendigkeits-Pruefung -----------------------------------------------------------
if not exist "%DEPLOY%\platforms\qwindows*.dll" set "ERR=platforms\qwindows.dll fehlt - das Programm wuerde nicht starten" & goto :fail
if not exist "%DEPLOY%\Qt6Multimedia*.dll" set "ERR=Qt6Multimedia.dll fehlt" & goto :fail
if exist "%DEPLOY%\multimedia\ffmpegmediaplugin*.dll" goto :mm_ok
if exist "%DEPLOY%\multimedia\windowsmediaplugin*.dll" goto :mm_ok
echo [deploy] WARNUNG: kein Multimedia-Backend-Plugin gefunden - die Wiedergabe wuerde nicht funktionieren.
:mm_ok

rem ---- 6. Lizenztexte ---------------------------------------------------------------------------
if exist "%ROOT%\LICENSE" copy /Y "%ROOT%\LICENSE" "%DEPLOY%\LICENSE.txt" >nul
if exist "%ROOT%\THIRD_PARTY_LICENSES.md" copy /Y "%ROOT%\THIRD_PARTY_LICENSES.md" "%DEPLOY%\THIRD_PARTY_LICENSES.txt" >nul

rem ---- 7. Pruefung wie beim Anwender: Abhaengigkeiten + Start mit sauberem PATH --------------------
for %%I in ("%WDQ%") do set "QTBIN=%%~dpI"
set "QTBIN=%QTBIN:~0,-1%"
echo [deploy] Pruefe Deploy-Ordner (verify_deploy.ps1) ...
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify_deploy.ps1" -DeployDir "%DEPLOY%" -QtBin "%QTBIN%"
if errorlevel 1 set "ERR=Pruefung des Deploy-Ordners fehlgeschlagen (Details siehe [verify]-Zeilen oben)" & goto :fail

echo [deploy] fertig: %DEPLOY%
exit /b 0

:copy_runtime
if exist "%DEPLOY%\%1" exit /b 0
if not defined GXXDIR exit /b 0
if exist "%GXXDIR%%1" copy /Y "%GXXDIR%%1" "%DEPLOY%\" >nul
exit /b 0

:fail
echo [deploy] FEHLER: %ERR%
exit /b 1

:usage
echo Aufruf: deploy_windows.bat ^<exe^> ^<deployDir^> ^<windeployqt.exe^> ^<mingw^|msvc^> ^<projectRoot^> [release^|debug]
exit /b 2
