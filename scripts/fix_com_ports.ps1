# Permanently separate the OwlThree serial ports and install the Pico
# BOOTSEL WinUSB driver. Open an Administrator PowerShell yourself and run:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File C:\Git\wlcs15\WaveShare-RaspPico2w\RR-Railcom-Pico2W-LCC-Waveshare-LCD2in-CAN\scripts\fix_com_ports.ps1
#
# This script does not request elevation.

$ErrorActionPreference = "Stop"
$principal = [Security.Principal.WindowsPrincipal]::new(
    [Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    Write-Host "This window is not Administrator."
    Write-Host "Open Administrator PowerShell yourself, then run this file again."
    Write-Host "This script will not ask for administrator rights."
    exit 1
}

function Get-PortEntries {
    $roots = @(
        "HKLM:\SYSTEM\CurrentControlSet\Enum\USB",
        "HKLM:\SYSTEM\CurrentControlSet\Enum\FTDIBUS"
    )
    $found = @()
    foreach ($root in $roots) {
        if (-not (Test-Path $root)) { continue }
        Get-ChildItem $root -Recurse -ErrorAction SilentlyContinue | ForEach-Object {
            $params = Join-Path $_.PSPath "Device Parameters"
            if (-not (Test-Path $params)) { return }
            $port = (Get-ItemProperty -Path $params -Name PortName -ErrorAction SilentlyContinue).PortName
            if (-not $port) { return }
            $found += [pscustomobject]@{
                Port = $port
                Path = $params
                Key  = $_.PSPath
            }
        }
    }
    return $found
}

function Wanted-Port([string]$key) {
    $text = $key.ToUpperInvariant()
    if ($text -match "VID_0483&PID_5740") { return "COM3" }
    if ($text -match "VID_2E8A&PID_0009") { return "COM20" }
    if ($text -match "VID_1A86&PID_55D3") { return "COM11" }
    if ($text -match "VID_2E8A&PID_000A") { return "COM14" }
    if ($text -match "VID_2E8A&PID_000C") { return "COM12" }
    if ($text -match "VID_0403&PID_6001") { return "COM6" }
    return $null
}

function Next-Free([System.Collections.Generic.HashSet[string]]$used) {
    $n = 21
    while ($used.Contains("COM$n")) { $n++ }
    return "COM$n"
}

Write-Host "COM ports before the change:"
$entries = @(Get-PortEntries)
$entries | Sort-Object Port, Key | ForEach-Object { Write-Host ("  {0,-6} {1}" -f $_.Port, $_.Key) }

$used = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$ordered = $entries | Sort-Object { if ((Wanted-Port $_.Key) -eq "COM3") { 0 } else { 1 } }, Key
$changes = @()
foreach ($entry in $ordered) {
    $want = Wanted-Port $entry.Key
    if ($want -and $used.Contains($want)) {
        $want = Next-Free $used
    }
    if (-not $want -and $entry.Port -eq "COM3") {
        $want = Next-Free $used
    }
    if ($want) { [void]$used.Add($want) }
    if ($want -and ($entry.Port -ne $want)) {
        Set-ItemProperty -Path $entry.Path -Name PortName -Value $want
        $changes += "  $($entry.Port) -> $want"
        Write-Host "set $want"
        Write-Host "  $($entry.Key)"
    }
}

if (-not $changes) {
    Write-Host "No COM port numbers needed to change."
} else {
    Write-Host "Changed:"
    $changes | ForEach-Object { Write-Host $_ }
}

$present = @(Get-PnpDevice -Class Ports -PresentOnly -ErrorAction SilentlyContinue)
foreach ($device in $present) {
    if ($device.InstanceId -match "VID_0483&PID_5740|VID_2E8A&PID_0009|VID_2E8A&PID_000A|VID_1A86&PID_55D3") {
        Write-Host "restart $($device.FriendlyName)"
        Disable-PnpDevice -InstanceId $device.InstanceId -Confirm:$false -ErrorAction Continue
        Start-Sleep -Seconds 1
        Enable-PnpDevice -InstanceId $device.InstanceId -Confirm:$false -ErrorAction Continue
    }
}

Write-Host ""
Write-Host "COM ports after the change:"
Get-PortEntries | Sort-Object Port, Key | ForEach-Object { Write-Host ("  {0,-6} {1}" -f $_.Port, $_.Key) }

$inf = Join-Path $PSScriptRoot "rp2boot_winusb.inf"
Write-Host ""
Write-Host "RP2 Boot driver:"
if (-not (Test-Path $inf)) {
    Write-Host "Missing $inf"
    exit 1
}
& pnputil.exe /add-driver $inf /install
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Windows did not install the local WinUSB package (it is not Microsoft-signed)."
    Write-Host "The mass-storage RPI-RP2 drive still works without this driver."
    Write-Host "Picotool needs WinUSB on the yellow RP2 Boot device:"
    Write-Host "  1. Hold BOOTSEL and plug in only the Pico W."
    Write-Host "  2. Download Zadig from https://zadig.akeo.ie/"
    Write-Host "  3. Options, List All Devices."
    Write-Host "  4. Select RP2 Boot (Interface 1)."
    Write-Host "  5. Driver WinUSB, then Install Driver."
    Write-Host "The CMSIS-DAP v2 Interface on the debug probe uses the same WinUSB choice."
    exit 0
}
Write-Host "RP2 Boot WinUSB package staged."
