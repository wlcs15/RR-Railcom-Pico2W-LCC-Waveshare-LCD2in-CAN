#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
python3 "$root/scripts/verify_wifi_wrap_picow.py"
rm -f "$root/build/firmware/pico_w_restouch/CMakeFiles/lcc_node.dir/src/lcc_main.c.o"
cmake --build "$root/build/firmware/pico_w_restouch"
echo "flash: picotool load -x -f $root/build/firmware/pico_w_restouch/lcc_node.elf"

