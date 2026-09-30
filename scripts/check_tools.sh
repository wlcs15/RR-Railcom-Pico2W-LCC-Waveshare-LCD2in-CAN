#!/usr/bin/env bash
# Host + firmware + on-target emulator presence check. Does not install anything.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
# Prefer workspace Debian/Renode wrappers when this tree is checked on the bot box.
if [[ -d /workspace/tools/bin ]]; then
  export PATH="/workspace/tools/bin:${PATH}"
fi
exec bash "$root/scripts/py_launch.sh" "check_tools.py" "$@"
