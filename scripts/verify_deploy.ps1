<#
.SYNOPSIS
  Prueft den Deploy-Ordner von WaveToAmiga auf Vollstaendigkeit - so, wie ein Anwender ihn vorfindet.

.DESCRIPTION
  Hintergrund: Auf dem Entwicklerrechner stehen Qt und MinGW im PATH. Ein Programm, dem im Ordner eine DLL
  fehlt, laeuft dort trotzdem (DLL wird ueber den PATH gefunden) - nach der Installation, beim Start ueber ein
  Icon, aber nicht. Dieses Skript findet solche Faelle schon beim Bauen:

   1. Abhaengigkeits-Pruefung: liest die Import-Tabellen aller .exe/.dll/Plugins im Ordner (objdump -p),
      kopiert fehlende, nicht zum System gehoerende DLLs aus dem Qt-/MinGW-bin-Ordner nach und meldet,
      was nirgends gefunden wird.
   2. Startpruefung: startet die deployte .exe mit  --selftest  und einem PATH, der NUR die Windows-Ordner
      enthaelt (kein Qt, kein MinGW). Das prueft Qt-Plattform-Plugin, Multimedia-Backend, Ressourcen, SoX-Konverter.

  Rueckgabewert 0 = alles in Ordnung, sonst 1 (der Build und damit der Installer brechen ab).
  Ueberspringen der Startpruefung: Umgebungsvariable W2A_SKIP_SELFTEST=1
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$DeployDir,
    [string]$QtBin = "",
    [string]$ExeName = "WaveToAmiga.exe",
    [int]$TimeoutSec = 40
)

$ErrorActionPreference = "Stop"
function Say([string]$m)  { Write-Host "[verify] $m" }
function Fail([string]$m) { Write-Host "[verify] FEHLER: $m" -ForegroundColor Red; exit 1 }

if (-not (Test-Path $DeployDir)) { Fail "Deploy-Ordner nicht gefunden: $DeployDir" }
$exe = Join-Path $DeployDir $ExeName
if (-not (Test-Path $exe)) { Fail "$ExeName fehlt im Deploy-Ordner" }

# ------------------------------------------------------------------ Werkzeuge / Suchordner
$objdump = $null
$c = Get-Command "objdump.exe" -ErrorAction SilentlyContinue
if ($c) { $objdump = $c.Source }
if (-not $objdump -and $env:MINGW_BIN) {
    $p = Join-Path $env:MINGW_BIN "objdump.exe"
    if (Test-Path $p) { $objdump = $p }
}

$searchDirs = @()
if ($QtBin) { $searchDirs += $QtBin }
if ($objdump) { $searchDirs += (Split-Path $objdump -Parent) }
if ($env:MINGW_BIN) { $searchDirs += $env:MINGW_BIN }
$gxx = Get-Command "g++.exe" -ErrorAction SilentlyContinue
if ($gxx) { $searchDirs += (Split-Path $gxx.Source -Parent) }
$searchDirs = @($searchDirs | Where-Object { $_ -and (Test-Path $_) } | Select-Object -Unique)

# ------------------------------------------------------------------ 1. Abhaengigkeiten
function Get-Imports([string]$file) {
    $names = @()
    $out = & $objdump -p $file 2>$null
    foreach ($line in $out) {
        if ($line -match 'DLL Name:\s*(\S+)') { $names += $Matches[1] }
    }
    return $names
}

function Test-SystemDll([string]$name) {
    $n = $name.ToLower()
    if ($n -like "api-ms-win-*" -or $n -like "ext-ms-win-*") { return $true }
    foreach ($d in @((Join-Path $env:SystemRoot "System32"), $env:SystemRoot)) {
        if (Test-Path (Join-Path $d $name)) { return $true }
    }
    return $false
}

if (-not $objdump) {
    Write-Host "[verify] WARNUNG: objdump.exe nicht gefunden - Abhaengigkeitspruefung uebersprungen (MinGW-bin in den PATH oder MINGW_BIN setzen)." -ForegroundColor Yellow
} else {
    Say "Pruefe Abhaengigkeiten aller DLLs/Plugins (Werkzeug: $objdump)"
    $queue = New-Object System.Collections.Generic.Queue[string]
    Get-ChildItem $DeployDir -Recurse -File | Where-Object { $_.Extension -in ".dll", ".exe" } |
        ForEach-Object { $queue.Enqueue($_.FullName) }
    $scanned = @{}
    $copied = @()
    $missing = @{}

    while ($queue.Count -gt 0) {
        $file = $queue.Dequeue()
        $key = $file.ToLower()
        if ($scanned.ContainsKey($key)) { continue }
        $scanned[$key] = $true

        foreach ($imp in (Get-Imports $file)) {
            if (Test-Path (Join-Path $DeployDir $imp)) { continue }
            if (Test-SystemDll $imp) { continue }
            $found = $null
            foreach ($d in $searchDirs) {
                $cand = Join-Path $d $imp
                if (Test-Path $cand) { $found = $cand; break }
            }
            if ($found) {
                Copy-Item $found $DeployDir -Force
                $copied += $imp
                $queue.Enqueue((Join-Path $DeployDir $imp))
            } else {
                $missing[$imp] = (Split-Path $file -Leaf)
            }
        }
    }

    if ($copied.Count -gt 0) {
        Write-Host ("[verify] Fehlende Abhaengigkeiten nachkopiert: " + (($copied | Select-Object -Unique) -join ", ")) -ForegroundColor Yellow
    }
    if ($missing.Count -gt 0) {
        foreach ($k in $missing.Keys) { Write-Host ("[verify]   nirgends gefunden: {0}  (benoetigt von {1})" -f $k, $missing[$k]) -ForegroundColor Red }
        Fail "Es fehlen DLLs, die nur ueber einen PATH-Eintrag gefunden wuerden. Siehe Liste oben."
    }
    Say ("Abhaengigkeiten vollstaendig ({0} Dateien geprueft)" -f $scanned.Count)
}

# ------------------------------------------------------------------ 2. Startpruefung mit sauberem PATH
if ($env:W2A_SKIP_SELFTEST -eq "1") {
    Say "Startpruefung uebersprungen (W2A_SKIP_SELFTEST=1)"
    exit 0
}

$log = Join-Path $env:TEMP ("w2a_selftest_" + [guid]::NewGuid().ToString("N") + ".log")
$oldPath = $env:PATH
$env:PATH = (Join-Path $env:SystemRoot "System32") + ";" + $env:SystemRoot   # KEIN Qt, KEIN MinGW
$timedOut = $false
$proc = $null
try {
    Say "Starte $ExeName --selftest mit minimalem PATH ..."
    $proc = Start-Process -FilePath $exe -ArgumentList @("--selftest", ('"' + $log + '"')) `
                          -WorkingDirectory $DeployDir -PassThru -WindowStyle Hidden
    if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
        $timedOut = $true
        try { $proc.Kill() } catch { }
    }
} catch {
    $env:PATH = $oldPath
    Fail ("Das Programm konnte nicht gestartet werden: " + $_.Exception.Message)
} finally {
    $env:PATH = $oldPath
}

$logText = ""
if (Test-Path $log) { $logText = (Get-Content $log -Raw); Remove-Item $log -Force -ErrorAction SilentlyContinue }

if ($timedOut) {
    Fail "Das Programm hat sich nach $TimeoutSec s nicht beendet (vermutlich Windows-Fehlerdialog wegen einer fehlenden DLL)."
}
$code = $proc.ExitCode
if ($code -ne 0 -or $logText -notmatch "OK") {
    $hex = "0x{0:X8}" -f $code
    $hint = switch ($code) {
        -1073741515 { "STATUS_DLL_NOT_FOUND: eine DLL fehlt im Ordner" }
        -1073741701 { "STATUS_INVALID_IMAGE_FORMAT: 32/64-Bit-Mischung oder falsche DLL-Version" }
        -1073741511 { "STATUS_ENTRYPOINT_NOT_FOUND: DLL in falscher Version" }
        3           { "Multimedia-Backend (Plugin 'multimedia\*') konnte nicht geladen werden" }
        4           { "Ressourcen fehlen" }
        default     { "" }
    }
    if ($logText) { Write-Host $logText }
    Fail "Startpruefung fehlgeschlagen (Exit-Code $code / $hex). $hint"
}
Write-Host $logText.TrimEnd()
Say "Startpruefung bestanden: das Programm laeuft auch ohne Qt/MinGW im PATH."
exit 0
