param([Parameter(Mandatory)][string]$Dll)
$ErrorActionPreference = "Stop"
if (-not (Test-Path $Dll)) { throw "$Dll is missing" }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$dumpbin = Get-ChildItem (Join-Path $vs "VC\Tools\MSVC\*\bin\Hostx64\x86\dumpbin.exe") | Select-Object -First 1
if (-not $dumpbin) { throw "dumpbin.exe not found" }
$asm = & $dumpbin.FullName /disasm:nobytes /nologo $Dll
$fninit = @($asm | Select-String -Pattern '\bfninit\b').Count
$emms = @($asm | Select-String -Pattern '\bemms\b').Count
"og3_patch.dll: fninit $fninit, emms $emms"
if ($fninit -gt 0) { throw "og3_patch.dll contains fninit - CLEAR_FPU_STACK=emms is missing from the AngelScript build" }
