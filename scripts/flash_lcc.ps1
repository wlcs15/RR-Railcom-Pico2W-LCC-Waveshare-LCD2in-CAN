# powershell -NoProfile -ExecutionPolicy Bypass -File scripts\flash_lcc.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }
& $py -u (Join-Path $Root "scripts\flash_lcc.py") @args
exit $LASTEXITCODE
