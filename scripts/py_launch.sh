#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
script="$1"
shift
if command -v python >/dev/null 2>&1; then
  exec python -u "$root/scripts/$script" "$@"
fi
if command -v python3 >/dev/null 2>&1; then
  exec python3 -u "$root/scripts/$script" "$@"
fi
echo "python not found"
exit 1
