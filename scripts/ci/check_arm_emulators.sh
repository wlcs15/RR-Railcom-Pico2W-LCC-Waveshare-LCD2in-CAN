#!/usr/bin/env bash
# GrokBot-CI-ARM local preflight: verify workspace Debian/Renode ARM emulators.
# Does not install anything. Does NOT check Espressif QEMU or simavr (other bots).
# Usage: bash scripts/ci/check_arm_emulators.sh
set -uo pipefail

# Prefer workspace wrappers (LD_LIBRARY_PATH / portable home) via env
if [[ -f /workspace/env/emulators.sh ]]; then
  # shellcheck source=/dev/null
  source /workspace/env/emulators.sh
fi
export PATH="/workspace/tools/bin:${PATH:-}"

missing=0
warns=0

ok()   { printf "  FOUND    %-22s %s\n" "$1" "$2"; }
fail() { printf "  MISSING  %-22s %s\n" "$1" "$2"; missing=1; }
warn() { printf "  DEFERRED %-22s %s\n" "$1" "$2"; warns=1; }

have() { command -v "$1" >/dev/null 2>&1; }

echo "ARM emulator tools check  (workspace: /workspace/tools)"
echo "Policy: check qemu-system-arm, qemu-system-aarch64, Renode only."
echo ""

echo "=== Required for GrokBot-CI-ARM on-target smoke ==="

if have qemu-system-arm; then
  ver=$(qemu-system-arm --version 2>/dev/null | head -n1 || echo "present")
  path=$(command -v qemu-system-arm)
  ok qemu-system-arm "$ver ($path)"
else
  fail qemu-system-arm "expected /workspace/tools/bin/qemu-system-arm (see EMULATORS.txt)"
fi

# Wrapper / export consistency (Debian extract under tools/qemu)
if [[ -n "${QEMU_ARM:-}" && -x "$QEMU_ARM" ]]; then
  ok QEMU_ARM "$QEMU_ARM"
elif [[ -x /workspace/tools/bin/qemu-system-arm ]]; then
  ok QEMU_ARM "/workspace/tools/bin/qemu-system-arm (export missing; PATH ok)"
else
  fail QEMU_ARM "source /workspace/env/emulators.sh"
fi

if have renode; then
  # Renode --version prints a short banner
  ver=$(renode --version 2>/dev/null | head -n1 || echo "present")
  path=$(command -v renode)
  ok renode "$ver ($path)"
else
  fail renode "expected /workspace/tools/bin/renode (see EMULATORS.txt)"
fi

if [[ -n "${RENODE:-}" && -x "$RENODE" ]]; then
  ok RENODE "$RENODE"
elif [[ -x /workspace/tools/bin/renode ]]; then
  ok RENODE "/workspace/tools/bin/renode (export missing; PATH ok)"
else
  fail RENODE "source /workspace/env/emulators.sh"
fi

echo ""
echo "=== Companion Debian aarch64 softmmu (installed with qemu-system-arm) ==="

if have qemu-system-aarch64; then
  ver=$(qemu-system-aarch64 --version 2>/dev/null | head -n1 || echo "present")
  path=$(command -v qemu-system-aarch64)
  ok qemu-system-aarch64 "$ver ($path)"
else
  # Not used by current Pico cortex-m* matrix, but part of the workspace Debian extract.
  warn qemu-system-aarch64 "optional for Pico M0/M33 matrix; expected tools/bin wrapper"
fi

if [[ -n "${QEMU_AARCH64:-}" && -x "$QEMU_AARCH64" ]]; then
  ok QEMU_AARCH64 "$QEMU_AARCH64"
elif have qemu-system-aarch64; then
  ok QEMU_AARCH64 "$(command -v qemu-system-aarch64) (export missing; PATH ok)"
else
  warn QEMU_AARCH64 "source /workspace/env/emulators.sh if needed"
fi

echo ""
echo "=== Skipped (other CI bots) ==="
echo "  DEFERRED Espressif QEMU / qemu-system-xtensa / qemu-system-riscv32"
echo "  DEFERRED simavr"

echo ""
if [[ "$missing" -ne 0 ]]; then
  echo "RESULT: missing required ARM emulators"
  echo "Fix: ensure tools under /workspace/tools/{qemu,renode,bin} and source /workspace/env/emulators.sh"
  echo "Doc: /workspace/tools/EMULATORS.txt"
  exit 1
fi
if [[ "$warns" -ne 0 ]]; then
  echo "RESULT: required ARM emulators present (optional DEFERRED above)"
  exit 0
fi
echo "RESULT: required ARM emulators present"
exit 0
