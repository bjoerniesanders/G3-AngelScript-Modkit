# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
# Baut den OG3-Patch mit dem schlanken Loader (src/loader) in eine Gothic-3-Kopie ein bzw. aus. Eigenstaendig, ohne das OpenGothic3-Hauptrepo.
#   powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> [-BuildDir build32]      Einbau
#   powershell -NoProfile -File tools\install_loader.ps1 -Game <Spielordner> -Uninstall               Rueckbau aus der juengsten Sicherung
# Einbau: die vorhandene dbghelp.dll des Spiels wird als dbghelp_orig.dll beiseitegelegt (der Loader leitet MiniDumpWriteDump dorthin weiter), der Loader
# (build32\loader\dbghelp.dll) wird zu dbghelp.dll, og3_patch.dll und eine og3_patch.ini kommen dazu, ausserdem ein Mini-Mod und der Ordner og3\mods.
# Sicherung nach <Spiel>\og3_backup\loader_<Zeit>\. Ist dbghelp.dll schon ein Proxy (z. B. der des Hauptrepos) und gibt es kein dbghelp_orig.dll, bricht das Skript ab.
# Dateien werden geloescht und neu angelegt (Hardlinks zu anderen Spielkopien werden getrennt, nicht mitveraendert).
param([Parameter(Mandatory)][string]$Game, [switch]$Uninstall, [string]$BuildDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot   # og3-patch
if (-not $BuildDir) { $BuildDir = Join-Path (Split-Path -Parent $root) "build32" }
$dbg = Join-Path $Game "dbghelp.dll"
$dbgOrig = Join-Path $Game "dbghelp_orig.dll"
$backupRoot = Join-Path $Game "og3_backup"
if (-not (Test-Path (Join-Path $Game "Gothic3.exe"))) { throw "$Game enthaelt keine Gothic3.exe" }
if (Get-Process -Name Gothic3 -ErrorAction SilentlyContinue) { throw "Gothic3.exe laeuft - bitte zuerst beenden" }
function Replace-File([string]$src, [string]$dst) { if (Test-Path $dst) { Remove-Item $dst -Force }; Copy-Item $src $dst }
function Looks-Like-Og3([string]$file) {   # Loader und Proxy des Hauptrepos enthalten diese Zeichenketten
    $t = [Text.Encoding]::GetEncoding(1252).GetString([IO.File]::ReadAllBytes($file))
    return $t.Contains("og3_patch.dll") -or $t.Contains("og3_hooks.ini") -or $t.Contains("OG3-Loader")
}
function Is-Loader([string]$file) {   # nur der schlanke Loader dieses Repos (der Proxy des Hauptrepos hat diese Zeichenkette nicht)
    $t = [Text.Encoding]::GetEncoding(1252).GetString([IO.File]::ReadAllBytes($file))
    return $t.Contains("OG3-Loader")
}

if ($Uninstall) {
    # neueste Sicherung, deren dbghelp.dll NICHT der Loader ist (Original oder ein frueherer Proxy des Hauptrepos; ein zweiter Einbau sichert den Loader nicht)
    $last = Get-ChildItem $backupRoot -Directory -Filter "loader_*" -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        Where-Object { (Test-Path (Join-Path $_.FullName "dbghelp.dll")) -and -not (Is-Loader (Join-Path $_.FullName "dbghelp.dll")) } | Select-Object -First 1
    if ($last) {
        Replace-File (Join-Path $last.FullName "dbghelp.dll") $dbg
        "dbghelp.dll aus $($last.FullName) wiederhergestellt"
    } elseif (Test-Path $dbgOrig) {
        Replace-File $dbgOrig $dbg
        "dbghelp.dll aus dbghelp_orig.dll wiederhergestellt (keine Sicherung gefunden)"
    } else { throw "Weder Sicherung unter $backupRoot noch dbghelp_orig.dll gefunden" }
    foreach ($f in "og3_patch.dll", "og3_patch.map") { $p = Join-Path $Game $f; if (Test-Path $p) { Remove-Item $p -Force } }
    "og3_patch.dll entfernt (og3_patch.ini, og3\ und dbghelp_orig.dll bleiben liegen)"
    return
}

$loader = Join-Path $BuildDir "loader\dbghelp.dll"
$patchDll = Join-Path $BuildDir "patch\og3_patch.dll"
foreach ($f in $loader, $patchDll) { if (-not (Test-Path $f)) { throw "$f fehlt - zuerst bauen (cmake --build <Ordner> --config Release --target og3_loader og3_patch)" } }
if (-not (Test-Path $dbg)) { throw "$dbg fehlt - ist das ein Gothic-3-Ordner?" }
$haveOrig = Test-Path $dbgOrig
if (-not $haveOrig -and (Looks-Like-Og3 $dbg)) { throw "dbghelp.dll ist schon ein OG3-Proxy und dbghelp_orig.dll fehlt - das Original zuerst wiederherstellen" }

$bk = Join-Path $backupRoot ("loader_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force $bk | Out-Null
if (Is-Loader $dbg) { "dbghelp.dll ist schon der OG3-Loader: wird nicht noch einmal gesichert (Original: dbghelp_orig.dll und fruehere Sicherungen)" }
else { Copy-Item $dbg $bk; if (Looks-Like-Og3 $dbg) { "dbghelp.dll ist ein Proxy des Hauptrepos: gesichert, der Rueckbau stellt ihn wieder her" } }
$ini = Join-Path $Game "og3_patch.ini"
if (Test-Path $ini) { Copy-Item $ini $bk }
"Sicherung: $bk"
if (-not $haveOrig) { Copy-Item $dbg $dbgOrig; "Original-dbghelp.dll als dbghelp_orig.dll abgelegt" }
else { "dbghelp_orig.dll ist schon vorhanden und bleibt" }
Replace-File $loader $dbg
Replace-File $patchDll (Join-Path $Game "og3_patch.dll")
$map = Join-Path $BuildDir "patch\og3_patch.map"
if (Test-Path $map) { Replace-File $map (Join-Path $Game "og3_patch.map") }
if (-not (Test-Path $ini)) {
    Set-Content -Path $ini -Value "[patch]`r`nenable=1`r`nscript=og3\scripts\main.as`r`nhotreload=1`r`n[loader]`r`ncrashlog=1`r`n" -Encoding ASCII
    "og3_patch.ini angelegt"
} else { "Vorhandene og3_patch.ini bleibt" }
$scripts = Join-Path $Game "og3\scripts"
New-Item -ItemType Directory -Force $scripts | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Game "og3\mods") | Out-Null
if (-not (Test-Path (Join-Path $scripts "main.as"))) { Copy-Item (Join-Path $root "examples\scripts\main.as") $scripts; "Mini-Mod: $scripts\main.as" }
else { "Vorhandenes Skript bleibt: $scripts\main.as" }
"Fertig. Protokolle nach dem Start: $Game\og3_loader.log und $Game\og3_patch.log"
