# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 G3ASM authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
# Testet den G3ASM-Loader in einem Wegwerf-Ordner, ohne das Spiel und ohne den Spielordner zu aendern.
#   powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <Original-dbghelp.dll> [-BuildDir build32]
# SharedBase: Original (z. B. G:\Gothic3\SharedBase.dll) oder Nachbau. DbghelpOrig: die Original-dbghelp.dll des Spiels (G:\Gothic3\dbghelp_orig.dll).
param([Parameter(Mandatory)][string]$SharedBase, [Parameter(Mandatory)][string]$DbghelpOrig, [string]$BuildDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path (Split-Path -Parent $root) "build32" }
$loader = Join-Path $BuildDir "loader\dbghelp.dll"
$patch = Join-Path $BuildDir "g3asm\g3asm.dll"
$test = Join-Path $BuildDir "g3asm\g3asm_loader_test.exe"
foreach ($f in $SharedBase, $DbghelpOrig, $loader, $patch, $test) { if (-not (Test-Path $f)) { throw "$f fehlt" } }
$dir = Join-Path $env:TEMP ("g3asm_loader_test_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory $dir | Out-Null
try {
    Copy-Item $SharedBase (Join-Path $dir "SharedBase.dll")
    Copy-Item $DbghelpOrig (Join-Path $dir "dbghelp_orig.dll")
    Copy-Item $loader (Join-Path $dir "dbghelp.dll")
    Copy-Item $patch (Join-Path $dir "g3asm.dll")
    Set-Content -Path (Join-Path $dir "g3asm.ini") -Value "[g3asm]`r`nenable=1`r`nhotreload=0`r`n[loader]`r`ncrashlog=1`r`n" -Encoding ASCII
    & $test $dir
    $rc = $LASTEXITCODE
    "--- g3asm_loader.log"; Get-Content (Join-Path $dir "g3asm_loader.log") -ErrorAction SilentlyContinue
    exit $rc
} finally {
    Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
}
