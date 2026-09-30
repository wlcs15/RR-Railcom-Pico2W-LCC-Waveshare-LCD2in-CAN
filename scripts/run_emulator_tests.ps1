# Run firmware under qemu-system-arm (default) or Renode. Does NOT flash hardware.
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_emulator_tests.ps1
# Prefer /workspace/tools/bin on PATH when present (bot box).
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$wsBin = "/workspace/tools/bin"
if (Test-Path -LiteralPath $wsBin) {
    $env:PATH = "$wsBin" + [IO.Path]::PathSeparator + $env:PATH
}
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }
& $py -u (Join-Path $Root "scripts\run_emulator_tests.py") @args
exit $LASTEXITCODE
