#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"
exec bash "$root/scripts/py_launch.sh" "run_lizard.py" "$@"
