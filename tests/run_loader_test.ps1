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
# Faelle: aktueller ini-Abschnitt [g3asm]; frueherer Abschnitt [patch] (wird gelesen, wenn [g3asm] fehlt) einmal mit eigenem Skriptpfad
# (den liest die DLL) und einmal mit enable=0 (das liest der Loader); dasselbe mit [g3asm] in einer Datei mit UTF-8-BOM.
# Jeder Fall laeuft in einem eigenen Ordner und Prozess.
$cases = @(
    @{ Name = "Abschnitt [g3asm]"; Ini = "[g3asm]`r`nenable=1`r`nhotreload=0`r`n[loader]`r`ncrashlog=1`r`n"; Arg = "" },
    @{ Name = "frueherer Abschnitt [patch], Skriptpfad"; Ini = "[patch]`r`nenable=1`r`nhotreload=0`r`nscript=alt\probe_alt.as`r`n[loader]`r`ncrashlog=1`r`n"; Arg = "expect=probe_alt.as" },
    @{ Name = "frueherer Abschnitt [patch], enable=0"; Ini = "[patch]`r`nenable=0`r`n"; Arg = "disabled" },
    # g3asm.ini mit UTF-8-BOM: GetPrivateProfile* uebersah dann [g3asm] (Loader: enable=0, DLL: Skriptpfad)
    @{ Name = "UTF-8-BOM, [g3asm] enable=0"; Ini = "[g3asm]`r`nenable=0`r`n"; Arg = "disabled"; Bom = $true },
    @{ Name = "UTF-8-BOM, [g3asm] Skriptpfad"; Ini = "[g3asm]`r`nenable=1`r`nhotreload=0`r`nscript=alt\probe_alt.as`r`n"; Arg = "expect=probe_alt.as"; Bom = $true }
)
$failed = 0
foreach ($c in $cases) {
    "=== $($c.Name)"
    $dir = Join-Path $env:TEMP ("g3asm_loader_test_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
    New-Item -ItemType Directory $dir | Out-Null
    try {
        Copy-Item $SharedBase (Join-Path $dir "SharedBase.dll")
        Copy-Item $DbghelpOrig (Join-Path $dir "dbghelp_orig.dll")
        Copy-Item $loader (Join-Path $dir "dbghelp.dll")
        Copy-Item $patch (Join-Path $dir "g3asm.dll")
        [IO.File]::WriteAllText((Join-Path $dir "g3asm.ini"), $c.Ini, (New-Object Text.UTF8Encoding([bool]$c.Bom)))
        if ($c.Arg) { & $test $dir $c.Arg } else { & $test $dir }
        if ($LASTEXITCODE -ne 0) { $failed++ }
        "--- g3asm_loader.log"; Get-Content (Join-Path $dir "g3asm_loader.log") -ErrorAction SilentlyContinue
    } finally {
        Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
    }
}
if ($failed) { "FEHLGESCHLAGEN: $failed von $($cases.Count) Faellen"; exit 1 }
"alle Pruefungen bestanden (0 Fehler), $($cases.Count) Faelle"
exit 0
