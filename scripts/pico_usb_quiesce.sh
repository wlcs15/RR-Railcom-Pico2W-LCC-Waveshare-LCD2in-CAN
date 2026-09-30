#!/bin/bash
# Stop the kernel from driving a Raspberry Pi RP2040/RP2350 that is already
# on the bus, and print the USB layout. Run as root after the keyboard is
# usable again. This does not reset the host controller.
set -euo pipefail
if [[ "$(id -u)" -ne 0 ]]; then
    echo "Run as root: sudo $0" >&2
    exit 1
fi
echo "=== USB tree ==="
lsusb -t
echo "=== HID and hubs ==="
for d in /sys/bus/usb/devices/*; do
    [[ -f "$d/idVendor" ]] || continue
    printf '%s  %s:%s  %s\n' \
        "$(basename "$d")" \
        "$(cat "$d/idVendor")" "$(cat "$d/idProduct")" \
        "$(cat "$d/product" 2>/dev/null || echo '?')"
done
echo "=== RP Pico devices (2e8a) ==="
found=0
for d in /sys/bus/usb/devices/*; do
    [[ -f "$d/idVendor" ]] || continue
    [[ "$(cat "$d/idVendor")" == "2e8a" ]] || continue
    found=1
    echo "quiesce $(basename "$d") $(cat "$d/idProduct") $(cat "$d/product" 2>/dev/null || true)"
    echo 0 > "$d/authorized" || true
    echo "authorized=$(cat "$d/authorized")"
done
if [[ "$found" -eq 0 ]]; then
    echo "No 2e8a device is on the bus."
fi
echo
echo "To make the next plug-in less likely to reset the keyboard, add this"
echo "one line to the kernel command line and reboot:"
echo "  usbcore.quirks=2e8a:0009:gk,2e8a:000f:gk"
echo "g delays init. k turns off USB link power management."
echo "Put the Logitech receiver on the USB-C dock (bus 3). That controller"
echo "is not the laptop root hub on bus 1."
