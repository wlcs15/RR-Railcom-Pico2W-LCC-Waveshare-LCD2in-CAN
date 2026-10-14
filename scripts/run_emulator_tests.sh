#!/usr/bin/env bash
# Shared emulator smoke: Renode default, --qemu experimental (Pico ELF -> Lockup/134). Does NOT flash.
# Usage: bash scripts/run_emulator_tests.sh [--elf PATH] [--board B] [--timeout SEC] [--qemu] [--no-build]
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
if [[ -d /workspace/tools/bin ]]; then export PATH="/workspace/tools/bin:${PATH}"; fi
if [[ -f /workspace/env/emulators.sh ]]; then
  # shellcheck source=/dev/null
  source /workspace/env/emulators.sh
fi
exec bash "$root/scripts/py_launch.sh" "run_emulator_tests.py" "$@"
