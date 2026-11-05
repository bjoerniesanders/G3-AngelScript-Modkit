# Aktualisiert die Kopie in graphify-modding aus den Quellen (AngelScript-Teil) und baut den Graphen inkrementell neu:
#   powershell -NoProfile -File og3-patch	ools\graphify_modding_sync.ps1   (danach im Ordner graphify-modding: /graphify . --update)
$src = Split-Path -Parent $PSScriptRoot; $dst = Join-Path $src 'graphify-modding'
New-Item -ItemType Directory -Force "$dst\src_patch","$dst\mods","$dst\docs" | Out-Null
Copy-Item "$src\src\*" "$dst\src_patch" -Recurse -Force
Get-ChildItem "$src\examples" -Recurse -Filter *.as | ForEach-Object { $rel=$_.FullName.Substring("$src\examples\".Length).Replace('\','__'); Copy-Item $_.FullName "$dst\mods\$rel.txt" -Force }
Get-ChildItem "$src\tests" -Recurse -Filter *.as | ForEach-Object { $rel=$_.FullName.Substring("$src\tests\".Length).Replace('\','__'); Copy-Item $_.FullName "$dst\mods\test__$rel.txt" -Force }
Copy-Item "$src\docs\modkit\*.md" "$dst\docs" -Force; Copy-Item "$src\docs\og3_patch.md" "$dst\docs" -Force
"Kopie aktualisiert: $dst"