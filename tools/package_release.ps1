param([Parameter(Mandatory)][string]$BuildDir, [Parameter(Mandatory)][string]$Version, [string]$OutDir = "dist")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
if (-not [IO.Path]::IsPathRooted($BuildDir)) { $BuildDir = Join-Path $root $BuildDir }
if (-not [IO.Path]::IsPathRooted($OutDir)) { $OutDir = Join-Path $root $OutDir }

$need = @("loader\dbghelp.dll", "patch\og3_patch.dll", "patch\og3_packmod.exe")
foreach ($f in $need) { if (-not (Test-Path (Join-Path $BuildDir $f))) { throw "$f is missing in $BuildDir (build first)" } }

$name = "og3-patch-$Version"
$stage = Join-Path $OutDir $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $stage "bin\loader"), (Join-Path $stage "bin\patch"), (Join-Path $stage "tools"), (Join-Path $stage "docs") | Out-Null

Copy-Item (Join-Path $BuildDir "loader\dbghelp.dll") (Join-Path $stage "bin\loader\")
foreach ($f in "og3_patch.dll", "og3_patch.map", "og3_packmod.exe") {
    $p = Join-Path $BuildDir "patch\$f"
    if (Test-Path $p) { Copy-Item $p (Join-Path $stage "bin\patch\") }
}
Copy-Item (Join-Path $root "tools\install_loader.ps1"), (Join-Path $root "tools\g3pak_read.py") (Join-Path $stage "tools\")
Copy-Item (Join-Path $root "examples") $stage -Recurse
Copy-Item (Join-Path $root "docs\modkit") (Join-Path $stage "docs") -Recurse
# Private planning files live next to the public docs on the maintainer's machine only (git-excluded); never ship them in a package.
foreach ($p in "roadmap.md", "plugin-analyse.md") { Remove-Item (Join-Path $stage "docs\modkit\$p") -Force -ErrorAction SilentlyContinue }
foreach ($f in "README.md", "LICENSE") { if (Test-Path (Join-Path $root $f)) { Copy-Item (Join-Path $root $f) $stage } }

$install = @"
OG3-Patch $Version - AngelScript modkit for Gothic 3 (original game, 32-bit)

Install (close Gothic 3 first; the folder must contain Gothic3.exe):
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "C:\Path\to\Gothic 3" -BuildDir bin

Uninstall:
  powershell -NoProfile -ExecutionPolicy Bypass -File tools\install_loader.ps1 -Game "C:\Path\to\Gothic 3" -Uninstall

Example mods: copy examples\mods\<name> to <Gothic 3>\og3\mods\ and enable it with a line "+<name>" in og3\mods\load_order.txt.
Documentation: docs\modkit\index.md
"@
[IO.File]::WriteAllText((Join-Path $stage "INSTALL.txt"), $install.Replace("`n", "`r`n"), (New-Object Text.UTF8Encoding($false)))

$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip
"Package: $zip ($([math]::Round((Get-Item $zip).Length / 1KB)) KB)"
