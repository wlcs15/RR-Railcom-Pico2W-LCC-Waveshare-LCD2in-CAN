# powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check_tools.ps1
# Host + firmware + on-target emulators (qemu-system-arm, renode required).
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
# Prefer a local workspace tools\bin if present (parity with Linux /workspace/tools/bin).
$wsBin = "/workspace/tools/bin"
if (Test-Path -LiteralPath $wsBin) {
    $env:PATH = "$wsBin" + [IO.Path]::PathSeparator + $env:PATH
}
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }
& $py -u (Join-Path $Root "scripts\check_tools.py") @args
exit $LASTEXITCODE
