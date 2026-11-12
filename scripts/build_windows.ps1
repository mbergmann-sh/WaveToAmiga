<#
.SYNOPSIS
  Baut WaveToAmiga mit qmake6, deployt die Qt6-Bibliotheken mit windeployqt und
  erstellt (optional) den Qt-Installer-Framework-Installer.

.DESCRIPTION
  Ablauf:  qmake6 -> make (SoX-Handler werden statisch eingebunden)
           -> Post-Link-Schritt scripts\deploy_windows.bat (von qmake automatisch ausgefuehrt):
                .exe + windeployqt (Qt6-DLLs, Plugins, FFmpeg) + MinGW-Laufzeit
                -> build\release\deploy
           -> Pruefung: vollstaendig, keine .o/.obj/moc_*/Quelltext im Deploy-Ordner
           -> Portable-ZIP -> Installer (binarycreator)

  Der Installer wird erst NACH dem Deploy-Schritt gebaut und enthaelt ausschliesslich den Inhalt
  von build\release\deploy (Programm + Bibliotheken + Lizenztexte) - nie Zwischendateien oder Quellcode.

.EXAMPLE
  # MinGW-Kit
  .\scripts\build_windows.ps1 -QtDir C:\Qt\6.8.1\mingw_64 -ToolsDir C:\Qt\Tools\mingw1310_64\bin `
                              -IfwDir C:\Qt\Tools\QtInstallerFramework\4.8\bin
  # MSVC-Kit: in der "x64 Native Tools Command Prompt for VS" starten
  .\scripts\build_windows.ps1 -QtDir C:\Qt\6.8.1\msvc2022_64 -IfwDir C:\Qt\Tools\QtInstallerFramework\4.8\bin
#>
[CmdletBinding()]
param(
    [string]$QtDir = $env:QTDIR,   # Qt-Kit, z. B. C:\Qt\6.8.1\mingw_64
    [string]$ToolsDir = "",        # MinGW-bin-Ordner (nur bei MinGW-Kits noetig, falls nicht im PATH)
    [string]$IfwDir = "",          # Qt Installer Framework bin-Ordner (binarycreator.exe)
    [switch]$NoInstaller,          # nur bauen + deployen
    [switch]$Clean                 # build\ und dist\ vorher loeschen
)

$ErrorActionPreference = "Stop"
$Root    = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Version = "1.0.0"
$Build   = Join-Path $Root "build\release"
$Dist    = Join-Path $Root "dist"
$Stage   = Join-Path $Build "deploy"          # wird vom qmake-Post-Link-Schritt gefuellt
$PkgData = Join-Path $Root "installer\packages\de.wavetoamiga.app\data"

function Step($t) { Write-Host "`n=== $t ===" -ForegroundColor Cyan }
function Run([string]$exe, [string[]]$a) {
    & $exe @a
    if ($LASTEXITCODE -ne 0) { throw "Fehler ($LASTEXITCODE): $exe $($a -join ' ')" }
}
function Find-Tool([string[]]$names) {
    foreach ($n in $names) { $c = Get-Command $n -ErrorAction SilentlyContinue; if ($c) { return $c.Source } }
    return $null
}

# ---------------------------------------------------------------- Umgebung
if ($QtDir)    { $env:PATH = (Join-Path $QtDir "bin") + ";" + $env:PATH }
if ($ToolsDir) { $env:PATH = $ToolsDir + ";" + $env:PATH }
if ($IfwDir)   { $env:PATH = $IfwDir + ";" + $env:PATH }

$qmake = Find-Tool @("qmake6", "qmake")
if (-not $qmake) { throw "qmake6 nicht gefunden. Bitte -QtDir angeben (z. B. C:\Qt\6.8.1\mingw_64)." }
$qtVer = (& $qmake -query QT_VERSION).Trim()
if ([int]($qtVer.Split('.')[0]) -lt 6) { throw "Qt 6 erforderlich, gefunden: $qtVer ($qmake)" }
$spec = (& $qmake -query QMAKE_XSPEC).Trim()
Write-Host "qmake: $qmake  (Qt $qtVer, Spec $spec)"

if ($spec -match "msvc") {
    $make = Find-Tool @("jom", "nmake")
    $makeArgs = @()
} else {
    $make = Find-Tool @("mingw32-make", "make")
    $makeArgs = @("-j$env:NUMBER_OF_PROCESSORS")
}
if (-not $make) { throw "Kein make-Programm gefunden (mingw32-make / jom / nmake). MSVC: 'Native Tools Command Prompt' benutzen; MinGW: -ToolsDir angeben." }

$deploy = Find-Tool @("windeployqt6", "windeployqt")
if (-not $deploy) { throw "windeployqt nicht gefunden (liegt in <Qt-Kit>\bin)." }

if ($Clean) {
    Step "Aufraeumen"
    Remove-Item -Recurse -Force $Build, $Dist -ErrorAction SilentlyContinue
}

# ---------------------------------------------------------------- Bauen
Step "qmake"
New-Item -ItemType Directory -Force $Build | Out-Null
Push-Location $Build
try {
    $qargs = @((Join-Path $Root "WaveToAmiga.pro"), "CONFIG+=release")
    if ($NoInstaller) { $qargs += "CONFIG+=no_installer" }
    Run $qmake $qargs
    Step "Kompilieren ($make)"
    Run $make $makeArgs
} finally { Pop-Location }

$exe = Join-Path $Build "bin\WaveToAmiga.exe"
if (-not (Test-Path $exe)) { throw "Build fehlgeschlagen: $exe nicht gefunden." }

Step "Selbsttest (WAV -> 8SVX, AIFF, 16SV, MAUD mit libsox)"
$test = Join-Path $Build "bin\test_converter.exe"
if (Test-Path $test) { Run $test @() } else { Write-Warning "test_converter.exe nicht gebaut - uebersprungen." }

# ---------------------------------------------------------------- Deploy-Ordner (windeployqt)
# Der Ordner entsteht bereits beim Linken (QMAKE_POST_LINK -> scripts\deploy_windows.bat).
# Fehlt er (z. B. weil mit CONFIG+=no_deploy gebaut wurde), wird der Schritt hier nachgeholt.
Step "Deploy-Ordner pruefen: $Stage"
if (-not (Test-Path (Join-Path $Stage "WaveToAmiga.exe"))) {
    Write-Host "Deploy-Ordner fehlt - fuehre deploy_windows.bat aus ..."
    $comp = if ($spec -match "msvc") { "msvc" } else { "mingw" }
    & (Join-Path $PSScriptRoot "deploy_windows.bat") $exe $Stage $deploy $comp $Root
    if ($LASTEXITCODE -ne 0) { throw "deploy_windows.bat ist fehlgeschlagen ($LASTEXITCODE)" }
}

# Pflichtinhalt pruefen, bevor irgendein Paket gebaut wird
$required = @("WaveToAmiga.exe", "platforms\qwindows.dll", "Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll", "Qt6Multimedia.dll")
if ($spec -notmatch "msvc") { $required += @("libstdc++-6.dll", "libwinpthread-1.dll") }
$missing = $required | Where-Object { -not (Test-Path (Join-Path $Stage $_)) }
if ($missing) { throw "Im Deploy-Ordner fehlen: $($missing -join ', ')" }
Write-Host "Pflichtdateien vorhanden:" ($required -join ", ")

# ---------------------------------------------------------------- Sicherheitspruefung
Step "Pruefe Deploy-Ordner (keine Build-Zwischendateien / kein Quellcode)"
$forbiddenExt  = @(".o", ".obj", ".a", ".lib", ".cpp", ".c", ".h", ".hpp", ".pro", ".pri", ".qrc", ".moc", ".pdb", ".ilk", ".exp")
$forbidden = Get-ChildItem $Stage -Recurse -File | Where-Object {
    $forbiddenExt -contains $_.Extension.ToLower() -or $_.Name -like "moc_*" -or $_.Name -like "qrc_*" -or $_.Name -like "ui_*"
}
if ($forbidden) {
    $forbidden | ForEach-Object { Write-Host "  unerwuenscht: $($_.FullName)" -ForegroundColor Red }
    throw "Unerwuenschte Dateien im Deploy-Ordner - Abbruch."
}
$files = Get-ChildItem $Stage -Recurse -File
"{0} Dateien, {1:N1} MB - OK" -f $files.Count, (($files | Measure-Object Length -Sum).Sum / 1MB) | Write-Host

# ---------------------------------------------------------------- Portable ZIP
Step "Portable-ZIP"
New-Item -ItemType Directory -Force $Dist | Out-Null
$zip = Join-Path $Dist "WaveToAmiga-$Version-windows-portable.zip"
Remove-Item -Force $zip -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $Stage "*") -DestinationPath $zip
Write-Host $zip

# ---------------------------------------------------------------- Installer
# Der Installer wird bereits beim Bauen erzeugt (QMAKE_POST_LINK -> scripts\make_installer_windows.bat),
# und zwar erst nach dem Deploy-Schritt. Hier wird nur noch geprueft und nach dist\ kopiert.
if ($NoInstaller) { Write-Host "`nFertig (ohne Installer)." -ForegroundColor Green; return }

$setupName = "WaveToAmigaSetup_IFW.exe"
$setup = Join-Path $Root "installer\output\$setupName"
if (-not (Test-Path $setup)) {
    throw "Installer nicht erzeugt: $setup`nbinarycreator.exe (Qt Installer Framework) nicht gefunden? -IfwDir angeben, z. B. C:\Qt\Tools\QtInstallerFramework\4.8\bin"
}
Copy-Item $setup (Join-Path $Dist "WaveToAmiga-$Version-windows-setup.exe") -Force
Write-Host "`nInstaller: $setup" -ForegroundColor Green
Write-Host "Kopie:     $(Join-Path $Dist "WaveToAmiga-$Version-windows-setup.exe")"
