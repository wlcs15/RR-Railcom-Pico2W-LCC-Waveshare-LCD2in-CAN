#!/usr/bin/env bash
# Run firmware under qemu-system-arm (default) or Renode. Does NOT flash hardware.
# Usage: bash scripts/run_emulator_tests.sh [--elf PATH] [--board pico2_w] [--renode] [--timeout SEC]
# Prefer: source /workspace/env/emulators.sh
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
if [[ -d /workspace/tools/bin ]]; then
  export PATH="/workspace/tools/bin:${PATH}"
fi
if [[ -f /workspace/env/emulators.sh ]]; then
  # shellcheck source=/dev/null
  source /workspace/env/emulators.sh
fi
exec bash "$root/scripts/py_launch.sh" "run_emulator_tests.py" "$@"
