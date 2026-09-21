# powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check_tools.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }
& $py -u (Join-Path $Root "scripts\check_tools.py") @args
exit $LASTEXITCODE
