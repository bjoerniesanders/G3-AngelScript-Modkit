# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 OG3-Patch authors. Zusatzerlaubnisse: LICENSE-EXCEPTIONS.md
# Runs clang-tidy (.clang-tidy in the repository root) over the patch sources. The Visual Studio generator writes no compile_commands.json,
# so the compiler flags are passed explicitly; the third-party include folders come from the CMake build folder.
#   powershell -NoProfile -File tools\run_clang_tidy.ps1 -BuildDir build32                  all sources
#   powershell -NoProfile -File tools\run_clang_tidy.ps1 -BuildDir build32 -Files src\draw_api.cpp,src\ai_trace.cpp
#   ... -Changed       only files that differ from HEAD (staged or not)
#   ... -Strict        exit code 1 if any warning is reported (for CI)
param([Parameter(Mandatory)][string]$BuildDir, [string[]]$Files = @(), [switch]$Changed, [switch]$Strict, [string]$ClangTidy = "")
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

function Find-ClangTidy {
    if ($ClangTidy) { return $ClangTidy }
    $cmd = Get-Command clang-tidy -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $candidates = @("$env:ProgramFiles\LLVM\bin\clang-tidy.exe")
    foreach ($vs in "${env:ProgramFiles(x86)}\Microsoft Visual Studio", "$env:ProgramFiles\Microsoft Visual Studio") {
        # x64 first: the 32-bit clang-tidy 15 shipped with Visual Studio 2022 crashes on these sources (access violation, seen 2026-11-13)
        if (Test-Path $vs) {
            $candidates += (Get-ChildItem "$vs\*\*\VC\Tools\Llvm\x64\bin\clang-tidy.exe" -ErrorAction SilentlyContinue).FullName
            $candidates += (Get-ChildItem "$vs\*\*\VC\Tools\Llvm\bin\clang-tidy.exe" -ErrorAction SilentlyContinue).FullName
        }
    }
    foreach ($c in $candidates) { if ($c -and (Test-Path $c)) { return $c } }
    throw "clang-tidy not found (install LLVM or the Visual Studio component 'C++ Clang tools', or pass -ClangTidy <path>)"
}
# Third-party folder: CMake cache entry if it holds the header, otherwise search the build folder (FetchContent puts it under _deps).
function Find-Include([string]$cacheVar, [string]$relHeader, [string]$headerName) {
    $cache = Join-Path $BuildDir "CMakeCache.txt"
    if (Test-Path $cache) {
        $line = Select-String -Path $cache -Pattern "^${cacheVar}:[A-Z]+=(.*)$" | Select-Object -First 1
        if ($line) { $dir = $line.Matches[0].Groups[1].Value; if (Test-Path (Join-Path $dir $relHeader)) { return (Split-Path -Parent (Join-Path $dir $relHeader)) } }
    }
    $hit = Get-ChildItem $BuildDir -Recurse -Filter $headerName -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($hit) { return $hit.DirectoryName }
    throw "$headerName not found via $cacheVar or under $BuildDir (configure the build first: cmake -S . -B <dir> -A Win32)"
}

# clang needs the MSVC and Windows SDK headers. It normally finds them itself; without vswhere it does not, so outside a developer prompt
# the INCLUDE variable is taken from vcvarsall.bat (x86) of the first Visual Studio installation found.
function Import-MsvcInclude {
    if ($env:INCLUDE) { return }
    foreach ($vs in "${env:ProgramFiles(x86)}\Microsoft Visual Studio", "$env:ProgramFiles\Microsoft Visual Studio") {
        $bat = Get-ChildItem "$vs\*\*\VC\Auxiliary\Build\vcvarsall.bat" -ErrorAction SilentlyContinue | Select-Object -First 1
        if (-not $bat) { continue }
        $line = cmd /c "`"$($bat.FullName)`" x86 >nul 2>nul && set INCLUDE" 2>$null | Where-Object { $_ -like "INCLUDE=*" } | Select-Object -First 1
        if ($line) { $env:INCLUDE = $line.Substring(8); return }
    }
}
$tidy = Find-ClangTidy
Import-MsvcInclude
$asInc = Find-Include "OG3P_ANGELSCRIPT_DIR" "angelscript\include\angelscript.h" "angelscript.h"
$asAddOn = Join-Path (Split-Path -Parent (Split-Path -Parent $asInc)) "add_on"
$mhInc = Find-Include "OG3P_MINHOOK_DIR" "include\MinHook.h" "MinHook.h"

$Files = @($Files | ForEach-Object { $_ -split ',' } | Where-Object { $_ })   # powershell -File passes 'a,b' as one string
if ($Changed) {
    $Files = @(git -C $root diff --name-only HEAD -- "*.cpp") | Where-Object { $_ -and (Test-Path (Join-Path $root $_)) }
    if (-not $Files) { "No changed .cpp files."; exit 0 }
} elseif (-not $Files) {
    $Files = @(Get-ChildItem (Join-Path $root "src") -Recurse -Filter *.cpp) + @(Get-ChildItem (Join-Path $root "tests") -Filter *.cpp) +
        @(Get-ChildItem (Join-Path $root "tools") -Filter *.cpp) | ForEach-Object { $_.FullName }
}
# 32-bit MSVC target like the real build (inline assembler, __declspec(naked), SEH)
$flags = @("--target=i686-pc-windows-msvc", "-std=c++17", "-fms-extensions", "-fms-compatibility", "-Wno-everything",
    "-D_CRT_SECURE_NO_WARNINGS", "-DWIN32", "-D_WINDOWS", "-DNDEBUG",
    "-I", (Join-Path $root "src"), "-isystem", $asInc, "-isystem", $asAddOn, "-isystem", $mhInc)

"clang-tidy: $tidy"
$total = 0
$counts = @{}
$failed = @()
$seen = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($f in $Files) {
    $path = if ([IO.Path]::IsPathRooted($f)) { $f } else { Join-Path $root $f }
    $ErrorActionPreference = "Continue"
    $out = & $tidy --quiet $path "--" @flags 2>&1 | ForEach-Object { "$_" }
    $rc = $LASTEXITCODE
    $ErrorActionPreference = "Stop"
    # A file that does not compile or a crashing clang-tidy must not look like "no findings"
    $out = @($out)   # a single output line would otherwise be a string, and -match on it a bool
    $broken = @($out | Where-Object { $_ -match 'clang-diagnostic-error|Error while processing|Stack dump:' }).Count -gt 0
    if ($broken -or ($rc -ne 0 -and @($out | Where-Object { $_ -match ': warning: ' }).Count -eq 0)) { $failed += (Split-Path -Leaf $path) }
    # Findings in headers come once per including file: count each only the first time
    $hits = @($out | Where-Object { $_ -match '(^|: )(warning|error): ' } | Where-Object { $seen.Add($_) })
    foreach ($h in $hits) { if ($h -match '\[([a-zA-Z0-9.,-]+)\]\s*$') { foreach ($c in $Matches[1].Split(',')) { $counts[$c] = 1 + [int]$counts[$c] } } }
    $total += $hits.Count
    "{0,-40} {1,4}" -f (Split-Path -Leaf $path), $hits.Count
    # Full clang-tidy output (with code excerpts) only for files that add something new or failed
    if ($hits.Count -gt 0 -or $failed -contains (Split-Path -Leaf $path)) {
        $out | Where-Object { $_ -notmatch '^\d+ warnings? generated' -and $_ -notmatch '^Suppressed \d+ warnings' -and $_ -notmatch '^Use -header-filter' } | ForEach-Object { "  $_" }
    }
}
""
"Findings by check:"
$counts.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object { "{0,5}  {1}" -f $_.Value, $_.Key }
"Total: $total"
if ($failed) { "NOT ANALYSED (compile error or clang-tidy crash): " + ($failed -join ", "); exit 2 }
if ($Strict -and $total -gt 0) { exit 1 }
exit 0
