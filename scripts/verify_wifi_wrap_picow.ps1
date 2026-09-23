$root = Split-Path -Parent $PSScriptRoot
if (-not $root) { $root = (Resolve-Path "$PSScriptRoot\..").Path }
Set-Location $root
python3 "$root\scripts\verify_wifi_wrap_picow.py"
Remove-Item -ErrorAction SilentlyContinue "$root\build\firmware\pico_w_restouch\CMakeFiles\lcc_node.dir\src\lcc_main.c.o"
cmake --build "$root\build\firmware\pico_w_restouch"
Write-Host "flash: picotool load -x -f $root\build\firmware\pico_w_restouch\lcc_node.elf"

