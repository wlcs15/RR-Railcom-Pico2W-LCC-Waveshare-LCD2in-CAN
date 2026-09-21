#!/usr/bin/env python3
"""Write local/wifi_psk_wrap.inc from a TTY. The password is not stored in git."""
from __future__ import print_function

import getpass
import hmac
import os
import sys
from hashlib import sha256

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError:
    sys.stderr.write("Python package cryptography is required\n")
    raise SystemExit(1)

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "local", "wifi_psk_wrap.inc")
SALT = b"owlthree-pico2w-wifi-wrap-v1"
# This Pico 2 W, from its USB self-check. Not a password.
MAC = bytes.fromhex("2CCF67E7AF5C")
UID = bytes.fromhex("DB6C98D898C72CF7")
NODE = bytes.fromhex("05010101A505")


def derive_key():
    ikm = UID + MAC + NODE
    info = bytes([0x05, 0x01, 0x01, 0x01, 0xA5]) + MAC
    prk = hmac.new(SALT, ikm, sha256).digest()
    block = 1
    prev = b""
    okm = b""
    while len(okm) < 32:
        prev = hmac.new(prk, prev + info + bytes([block]), sha256).digest()
        okm += prev
        block += 1
    return okm[:32]


def main():
    if not sys.stdin.isatty():
        sys.stderr.write("Run this from your own terminal. Do not pipe the password.\n")
        return 1
    ssid = input("Wi-Fi SSID: ").strip()
    if not ssid or len(ssid) > 32 or any(ch in ssid for ch in '"\\\n\r'):
        sys.stderr.write("SSID must be 1..32 characters without quotes or backslashes\n")
        return 1
    psk = getpass.getpass("Wi-Fi password (not echoed, not saved as text): ")
    if not psk or len(psk) > 64:
        sys.stderr.write("Password must be 1..64 bytes\n")
        return 1
    nonce = os.urandom(12)
    packed = AESGCM(derive_key()).encrypt(nonce, psk.encode("utf-8"), None)
    tag, cipher = packed[-16:], packed[:-16]
    blob = bytes([1]) + nonce + tag + bytes([len(psk)]) + cipher.ljust(64, b"\x00")
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    hex_bytes = ", ".join("0x%02X" % b for b in blob)
    with open(OUT, "w", encoding="utf-8") as handle:
        handle.write("/* Ciphertext only. Do not commit. */\n")
        handle.write('static const char kWifiWrapSsid[] = "%s";\n' % ssid)
        handle.write("static const uint8_t kWifiWrapBlob[%d] = { %s };\n" % (len(blob), hex_bytes))
    print("Wrote %s" % OUT)
    print("Rebuild with scripts/build_firmware.sh and flash the Pico 2 W image.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
