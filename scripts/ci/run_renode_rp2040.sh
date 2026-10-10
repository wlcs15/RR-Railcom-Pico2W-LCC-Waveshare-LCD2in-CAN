#!/usr/bin/env bash
# Smoke-run RP2040 ELF under Renode (minimal Cortex-M0 map; community Renode_RP2040 needs custom plugins).
set -uo pipefail
source /workspace/env/emulators.sh
if ! command -v renode >/dev/null 2>&1; then
  echo "EMU_RENODE_RP2040 missing renode (source /workspace/env/emulators.sh; see EMULATORS.txt)" >&2
  exit 2
fi
ELF="${1:?elf}"
TIMEOUT_SEC="${2:-90}"
LOG="${3:-/dev/stdout}"
if [[ ! -f "$ELF" ]]; then
  echo "EMU_RENODE_RP2040 missing ELF: $ELF" | tee -a "$LOG"
  exit 2
fi
ABS_ELF=$(readlink -f "$ELF")
RESC=$(mktemp /tmp/renode-rp2040-XXXX.resc)
# Minimal RP2040-like memory map (XIP @ 0x10000000, SRAM @ 0x20000000)
cat > "$RESC" << RESCEOF
using sysbus
mach create "rp2040_smoke"
machine LoadPlatformDescriptionFromString """
cpu: CPU.CortexM @ sysbus
    cpuType: "cortex-m0"
    nvic: nvic
nvic: IRQControllers.NVIC @ sysbus 0xE000E000
    -> cpu@0
flash: Memory.MappedMemory @ sysbus 0x10000000
    size: 0x00200000
sram: Memory.MappedMemory @ sysbus 0x20000000
    size: 0x00042000
"""
sysbus LoadELF @$ABS_ELF
cpu PC 0x10000000
emulation RunFor "00:00:02"
RESCEOF
set +e
timeout --signal=TERM --kill-after=15 "${TIMEOUT_SEC}s" \
  renode --disable-xwt --console -e "include @$RESC; quit" \
  < /dev/null > "${LOG}.renode.out" 2>&1
rc=$?
set -e
echo "EMU_RENODE_RP2040 elf=$ELF timeout=${TIMEOUT_SEC}s exit=$rc" | tee -a "$LOG"
tail -n 80 "${LOG}.renode.out" >> "$LOG" || true
rm -f "$RESC"
exit "$rc"
