# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
# Baut den OG3-Patch (AngelScript) in eine Spielkopie ein bzw. aus. Original-SharedBase, PU-Patch und Hybridbetrieb bleiben unveraendert.
#   powershell -NoProfile -File tools\install_patch.ps1 [-Game G:\Gothic3]      Einbau (Skript nur, wenn noch keins existiert)
#   powershell -NoProfile -File tools\install_patch.ps1 -Uninstall               Rueckbau aus der juengsten Sicherung
# Einbau: Sicherung von dbghelp.dll und og3_hooks.ini nach <Spiel>\og3_backup\patch_<Zeit>\; neuer Proxy (build32\hook\dbghelp.dll,
# gleiche Funktionen + [memprof]/[patch]), og3_patch.dll, og3\scripts\main.as (Mini-Mod); in og3_hooks.ini wird [patch] enable=1 gesetzt.
# Dateien werden geloescht und neu angelegt (Hardlinks zu G:\Gothic3_test/_og3test werden getrennt, nicht mitveraendert).
param([string]$Game = "G:\Gothic3", [switch]$Uninstall, [switch]$Script, [string]$BuildDir = "", [string]$ProxyDll = "")   # -Script: Mini-Mod-Vorlage ueber vorhandenes main.as (alte Fassung in die Sicherung)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot   # og3-patch
$main = Split-Path -Parent $root           # Hauptrepo (nur wenn og3-patch dort als Unterordner liegt)
if (-not $BuildDir) { $BuildDir = Join-Path $main "build32" }
# Proxy (dbghelp.dll mit [patch]-Abschnitt): -ProxyDll angeben oder aus dem Hauptrepo-Build nehmen
$proxy = Join-Path $Game "dbghelp.dll"
$ini = Join-Path $Game "og3_hooks.ini"
$backupRoot = Join-Path $Game "og3_backup"
if (Get-Process -Name Gothic3 -ErrorAction SilentlyContinue) { throw "Gothic3.exe laeuft - bitte zuerst beenden" }
function Replace-File([string]$src, [string]$dst) { if (Test-Path $dst) { Remove-Item $dst -Force }; Copy-Item $src $dst }

if ($Uninstall) {
    $last = Get-ChildItem $backupRoot -Directory -Filter "patch_*" -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1
    if (-not $last) { throw "Keine Patch-Sicherung unter $backupRoot" }
    Replace-File (Join-Path $last.FullName "dbghelp.dll") $proxy
    Replace-File (Join-Path $last.FullName "og3_hooks.ini") $ini
    if (Test-Path (Join-Path $Game "og3_patch.dll")) { Remove-Item (Join-Path $Game "og3_patch.dll") -Force }
    "Rueckbau aus $($last.FullName) (og3\scripts bleibt liegen)"
    return
}

$newProxy = if ($ProxyDll) { $ProxyDll } else { Join-Path $BuildDir "hook\dbghelp.dll" }
$patchDll = Join-Path $BuildDir "patch\og3_patch.dll"
foreach ($f in $newProxy, $patchDll) { if (-not (Test-Path $f)) { throw "$f fehlt - zuerst bauen (og3_dbghelp, og3_patch)" } }
$bk = Join-Path $backupRoot ("patch_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
New-Item -ItemType Directory -Force $bk | Out-Null
Copy-Item $proxy $bk
Copy-Item $ini $bk
"Sicherung: $bk"
Replace-File $newProxy $proxy
Replace-File $patchDll (Join-Path $Game "og3_patch.dll")
$scripts = Join-Path $Game "og3\scripts"
New-Item -ItemType Directory -Force $scripts | Out-Null
New-Item -ItemType Directory -Force (Join-Path $Game "og3\mods") | Out-Null
if ($Script -and (Test-Path (Join-Path $scripts "main.as"))) { Copy-Item (Join-Path $scripts "main.as") $bk; Remove-Item (Join-Path $scripts "main.as") -Force; "Altes main.as gesichert" }
if (-not (Test-Path (Join-Path $scripts "main.as"))) { Copy-Item (Join-Path $root "examples\scripts\main.as") $scripts; "Mini-Mod: $scripts\main.as" }
else { "Vorhandenes Skript bleibt: $scripts\main.as" }
$text = (Get-Content $ini -Raw) -replace '(?ms)^\[patch\].*?(?=^\[|\z)', ''
$text = $text.TrimEnd() + "`r`n[patch]`r`nenable=1`r`nscript=og3\scripts\main.as`r`nhotreload=1`r`n"
Remove-Item $ini -Force
Set-Content -Path $ini -Value $text -Encoding ASCII -NoNewline
"og3_hooks.ini: [patch] enable=1"
"Fertig. Protokoll nach dem Start: $Game\og3_patch.log"
