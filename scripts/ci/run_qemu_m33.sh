#!/usr/bin/env bash
set -uo pipefail
source /workspace/env/emulators.sh
if ! command -v qemu-system-arm >/dev/null 2>&1; then
  echo "EMU_QEMU_M33 missing qemu-system-arm (source /workspace/env/emulators.sh; see EMULATORS.txt)" >&2
  exit 2
fi
ELF="${1:?elf}"; TIMEOUT_SEC="${2:-90}"; LOG="${3:-/dev/stdout}"
[[ -f "$ELF" ]] || { echo "EMU_QEMU_M33 missing ELF: $ELF" | tee -a "$LOG"; exit 2; }
run_one() {
  local machine="$1"
  timeout --signal=TERM --kill-after=10 "${TIMEOUT_SEC}s" \
    qemu-system-arm -machine "$machine" -cpu cortex-m33 -kernel "$ELF" \
      -nographic -semihosting-config enable=on,target=native \
      -monitor none -serial mon:stdio \
      < /dev/null > "${LOG}.qemu.out" 2>&1
  return $?
}
set +e
run_one mps2-an505; rc=$?
if [[ $rc -eq 134 || $rc -eq 139 ]]; then
  echo "EMU_QEMU_M33 mps2-an505 crashed rc=$rc; trying musca-b1" | tee -a "$LOG"
  run_one musca-b1; rc=$?
fi
echo "EMU_QEMU_M33 elf=$ELF timeout=${TIMEOUT_SEC}s exit=$rc" | tee -a "$LOG"
tail -n 40 "${LOG}.qemu.out" >> "$LOG" 2>/dev/null || true
exit "$rc"
