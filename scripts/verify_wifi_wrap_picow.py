#!/usr/bin/env python3
"""Build a known wrap for the Pico W and check it decrypts on the host."""
from __future__ import print_function

import hmac
import os
import sys
from hashlib import sha256

from cryptography.hazmat.primitives.ciphers.aead import AESGCM

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
OUT = os.path.join(ROOT, "local", "wifi_psk_wrap.inc")
SALT = b"owlthree-pico2w-wifi-wrap-v1"
MAC = bytes.fromhex("88A29E015D50")
UID = bytes.fromhex("E6647C15674D422D")
NODE = bytes.fromhex("05010101A505")
SSID = "rr-wrap-test"
PSK = "test-psk-0000-bogus"

def derive_key():
    ikm = UID + MAC + NODE
    info = bytes([0x05, 0x01, 0x01, 0x01, 0xA5]) + MAC
    prk = hmac.new(SALT, ikm, sha256).digest()
    prev = b""
    okm = b""
    block = 1
    while len(okm) < 32:
        prev = hmac.new(prk, prev + info + bytes([block]), sha256).digest()
        okm += prev
        block += 1
    return okm[:32], ikm, info

def main():
    key, ikm, info = derive_key()
    nonce = os.urandom(12)
    packed = AESGCM(key).encrypt(nonce, PSK.encode("utf-8"), None)
    tag, cipher = packed[-16:], packed[:-16]
    blob = bytes([1]) + nonce + tag + bytes([len(PSK)]) + cipher.ljust(64, b"\x00")
    # Host decrypt must succeed before we ask the Pico to do it.
    plain = AESGCM(key).decrypt(nonce, cipher + tag, None)
    if plain.decode("utf-8") != PSK:
        print("host decrypt mismatch")
        return 1
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    hex_bytes = ", ".join("0x%02X" % b for b in blob)
    with open(OUT, "w", encoding="utf-8") as handle:
        handle.write("/* TEST wrap. Bogus PSK. Do not commit. */\n")
        handle.write('static const char kWifiWrapSsid[] = "%s";\n' % SSID)
        handle.write("static const uint8_t kWifiWrapBlob[%d] = { %s };\n" % (len(blob), hex_bytes))
    print("wrote", OUT)
    print("ikm", ikm.hex())
    print("info", info.hex())
    print("blob", " ".join("%02X" % b for b in blob[:14]))
    print("clen", blob[29])
    print("host decrypt ok")
    print("next: rm lcc_main.c.o, cmake --build pico_w_restouch, flash, expect this blob on serial")
    print("Wi-Fi join will fail (SSID is fake). Unwrap must NOT say gcm fail.")
    return 0

if __name__ == "__main__":
    sys.exit(main())

