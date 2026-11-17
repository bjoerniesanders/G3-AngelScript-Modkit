# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Additional permissions: LICENSE-EXCEPTIONS.md
# Installs G3ASM with the small loader (src/loader) into a Gothic 3 copy, or uninstalls it. Standalone, without the OpenGothic3 main repository.
#   powershell -NoProfile -File tools\install_loader.ps1 -Game <game folder> [-BuildDir build32]      install
#   powershell -NoProfile -File tools\install_loader.ps1 -Game <game folder> -Uninstall               uninstall from the newest backup
# Install: the script renames the existing dbghelp.dll of the game to dbghelp_orig.dll (the loader sends MiniDumpWriteDump to it). The loader
# (build32\loader\dbghelp.dll) becomes dbghelp.dll. g3asm.dll and a g3asm.ini are added, and also a mini mod and the folder g3asm\mods.
# Backup in <game>\g3asm_backup\loader_<time>\. If dbghelp.dll is already a proxy (for example the proxy of the main repository) and there is no dbghelp_orig.dll, the script stops.
# The script deletes files and makes them again (hard links to other game copies are separated, not changed).
# Requirements (checked only during the install): Community Patch 1.75 (PE time stamp of Gothic3.exe 4F959F4F) and Parallel Universe Patch (paru_s1.dll,
# scripts\Script_paru.dll). If something is missing, the script stops. -Force installs anyway (not tested).
param([Parameter(Mandatory)][string]$Game, [switch]$Uninstall, [string]$BuildDir = "", [switch]$Force)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot   # G3-AngelScript-Modkit
# without -BuildDir: build32 in the repository (as built in the README), if not there build32 next to the repository (build as a subproject of a parent CMake tree)
if (-not $BuildDir) {
    $BuildDir = Join-Path $root "build32"
    $outer = Join-Path (Split-Path -Parent $root) "build32"
    if (-not (Test-Path (Join-Path $BuildDir "g3asm\g3asm.dll")) -and (Test-Path (Join-Path $outer "g3asm\g3asm.dll"))) { $BuildDir = $outer }
}
$dbg = Join-Path $Game "dbghelp.dll"
$dbgOrig = Join-Path $Game "dbghelp_orig.dll"
$backupRoot = Join-Path $Game "g3asm_backup"
if (-not (Test-Path (Join-Path $Game "Gothic3.exe"))) { throw "$Game enthaelt keine Gothic3.exe" }
if (Get-Process -Name Gothic3 -ErrorAction SilentlyContinue) { throw "Gothic3.exe laeuft - bitte zuerst beenden" }
function Replace-File([string]$src, [string]$dst) { if (Test-Path $dst) { Remove-Item $dst -Force }; Copy-Item $src $dst }
function Looks-Like-Proxy([string]$file) {   # the loader and the proxy of the main repository contain these strings
    $t = [Text.Encoding]::GetEncoding(1252).GetString([IO.File]::ReadAllBytes($file))
    return $t.Contains("g3asm.dll") -or $t.Contains("og3_hooks.ini") -or $t.Contains("G3ASM-Loader")
}
function Is-Loader([string]$file) {   # only the small loader of this repository (the proxy of the main repository does not have these strings)
    $t = [Text.Encoding]::GetEncoding(1252).GetString([IO.File]::ReadAllBytes($file))
    return $t.Contains("G3ASM-Loader")
}

if ($Uninstall) {
    # newest backup whose dbghelp.dll is NOT the loader (the original or an earlier proxy of the main repository; a second install does not back up the loader)
    $last = Get-ChildItem $backupRoot -Directory -Filter "loader_*" -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        Where-Object { (Test-Path (Join-Path $_.FullName "dbghelp.dll")) -and -not (Is-Loader (Join-Path $_.FullName "dbghelp.dll")) } | Select-Object -First 1
    if ($last) {
        Replace-File (Join-Path $last.FullName "dbghelp.dll") $dbg
        "dbghelp.dll aus $($last.FullName) wiederhergestellt"
    } elseif (Test-Path $dbgOrig) {
        Replace-File $dbgOrig $dbg
        "dbghelp.dll aus dbghelp_orig.dll wiederhergestellt (keine Sicherung gefunden)"
    } else { throw "Weder Sicherung unter $backupRoot noch dbghelp_orig.dll gefunden" }
    foreach ($f in "g3asm.dll", "g3asm.map") { $p = Join-Path $Game $f; if (Test-Path $p) { Remove-Item $p -Force } }
    "g3asm.dll entfernt (g3asm.ini, g3asm\ und dbghelp_orig.dll bleiben liegen)"
    return
}

function Get-PeTimeStamp([string]$file) {
    $b = [IO.File]::ReadAllBytes($file)
    return [BitConverter]::ToUInt32($b, [BitConverter]::ToInt32($b, 0x3C) + 8)
}
$missing = @()
if ((Get-PeTimeStamp (Join-Path $Game "Gothic3.exe")) -ne 0x4F959F4F) { $missing += "Community Patch 1.75 (Gothic3.exe hat einen anderen Stand)" }
if (-not ((Test-Path (Join-Path $Game "paru_s1.dll")) -and (Test-Path (Join-Path $Game "scripts\Script_paru.dll")))) {
    $missing += "Parallel-Universe-Patch (paru_s1.dll oder scripts\Script_paru.dll fehlt)"
}
if ($missing) {
    $text = "Voraussetzung fehlt: " + ($missing -join "; ")
    if (-not $Force) { throw "$text. G3ASM braucht Community Patch 1.75 und Parallel-Universe-Patch. Mit -Force trotzdem einbauen (ungetestet)." }
    "WARNUNG: $text - Einbau wegen -Force trotzdem (ungetestet)"
} else { "Voraussetzungen: Community Patch 1.75 und Parallel-Universe-Patch gefunden" }

$loader = Join-Path $BuildDir "loader\dbghelp.dll"
$patchDll = Join-Path $BuildDir "g3asm\g3asm.dll"
foreach ($f in $loader, $patchDll) { if (-not (Test-Path $f)) { throw "$f fehlt - zuerst bauen (cmake --build <Ordner> --config Release --target g3asm_loader g3asm)" } }
if (-not (Test-Path $dbg)) { throw "$dbg fehlt - ist das ein Gothic-3-Ordner?" }
$haveOrig = Test-Path $dbgOrig
if (-not $haveOrig -and (Looks-Like-Proxy $dbg)) { throw "dbghelp.dll ist schon ein Proxy (Hauptrepo oder frueherer Loader) und dbghelp_orig.dll fehlt - das Original zuerst wiederherstellen" }

$bk = Join-Path $backupRoot ("loader_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force $bk | Out-Null
if (Is-Loader $dbg) { "dbghelp.dll ist schon der G3ASM-Loader: wird nicht noch einmal gesichert (Original: dbghelp_orig.dll und fruehere Sicherungen)" }
else { Copy-Item $dbg $bk; if (Looks-Like-Proxy $dbg) { "dbghelp.dll ist ein Proxy des Hauptrepos: gesichert, der Rueckbau stellt ihn wieder her" } }
$ini = Join-Path $Game "g3asm.ini"
if (Test-Path $ini) { Copy-Item $ini $bk }
"Sicherung: $bk"
if (-not $haveOrig) { Copy-Item $dbg $dbgOrig; "Original-dbghelp.dll als dbghelp_orig.dll abgelegt" }
else { "dbghelp_orig.dll ist schon vorhanden und bleibt" }
Replace-File $loader $dbg
Replace-File $patchDll (Join-Path $Game "g3asm.dll")
$map = Join-Path $BuildDir "g3asm\g3asm.map"
if (Test-Path $map) { Replace-File $map (Join-Path $Game "g3asm.map") }
if (-not (Test-Path $ini)) {
    Set-Content -Path $ini -Value "[g3asm]`r`nenable=1`r`nscript=g3asm\scripts\main.as`r`nhotreload=1`r`n[loader]`r`ncrashlog=1`r`n" -Encoding ASCII
    "g3asm.ini angelegt"
} else { "Vorhandene g3asm.ini bleibt" }
$scripts = Join-Path $Game "g3asm\scripts"
New-Item -ItemType Directory -Force $scripts | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Game "g3asm\mods") | Out-Null
if (-not (Test-Path (Join-Path $scripts "main.as"))) { Copy-Item (Join-Path $root "examples\scripts\main.as") $scripts; "Mini-Mod: $scripts\main.as" }
else { "Vorhandenes Skript bleibt: $scripts\main.as" }
"Fertig. Protokolle nach dem Start: $Game\g3asm_loader.log und $Game\g3asm.log"
