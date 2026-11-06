# Testet den OG3-Loader in einem Wegwerf-Ordner, ohne das Spiel und ohne den Spielordner zu aendern.
#   powershell -NoProfile -File tests\run_loader_test.ps1 -SharedBase <SharedBase.dll> -DbghelpOrig <Original-dbghelp.dll> [-BuildDir build32]
# SharedBase: Original (z. B. G:\Gothic3\SharedBase.dll) oder Nachbau. DbghelpOrig: die Original-dbghelp.dll des Spiels (G:\Gothic3\dbghelp_orig.dll).
param([Parameter(Mandatory)][string]$SharedBase, [Parameter(Mandatory)][string]$DbghelpOrig, [string]$BuildDir = "")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not $BuildDir) { $BuildDir = Join-Path (Split-Path -Parent $root) "build32" }
$loader = Join-Path $BuildDir "loader\dbghelp.dll"
$patch = Join-Path $BuildDir "patch\og3_patch.dll"
$test = Join-Path $BuildDir "patch\og3_loader_test.exe"
foreach ($f in $SharedBase, $DbghelpOrig, $loader, $patch, $test) { if (-not (Test-Path $f)) { throw "$f fehlt" } }
$dir = Join-Path $env:TEMP ("og3_loader_test_" + [guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory $dir | Out-Null
try {
    Copy-Item $SharedBase (Join-Path $dir "SharedBase.dll")
    Copy-Item $DbghelpOrig (Join-Path $dir "dbghelp_orig.dll")
    Copy-Item $loader (Join-Path $dir "dbghelp.dll")
    Copy-Item $patch (Join-Path $dir "og3_patch.dll")
    Set-Content -Path (Join-Path $dir "og3_patch.ini") -Value "[patch]`r`nenable=1`r`nhotreload=0`r`n[loader]`r`ncrashlog=1`r`n" -Encoding ASCII
    & $test $dir
    $rc = $LASTEXITCODE
    "--- og3_loader.log"; Get-Content (Join-Path $dir "og3_loader.log") -ErrorAction SilentlyContinue
    exit $rc
} finally {
    Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
}
