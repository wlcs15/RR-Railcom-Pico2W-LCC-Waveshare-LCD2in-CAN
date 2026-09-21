#!/usr/bin/env python3
"""Load the Pico 2 W image and read its USB self-check. No SWD probe."""
from __future__ import print_function

import glob
import os
import subprocess
import sys
import time

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UF2 = os.path.join(ROOT, "build", "firmware", "pico2_w", "lcc_node.uf2")
NEEDLES = (
    "TARGET node 05.01.01.01.A5.05",
    "TARGET pin conflicts 0",
    "OpenLCB Node ID: 05.01.01.01.A5.05",
)


def _ports():
    found = sorted(glob.glob("/dev/serial/by-id/*"))
    found.extend(sorted(glob.glob("/dev/ttyACM*")))
    return found


def _read_serial(seconds):
    text = ""
    for path in _ports():
        if "Arduino" in path or "STMicro" in path:
            continue
        try:
            handle = open(path, "rb", buffering=0)
            os.set_blocking(handle.fileno(), False)
        except OSError as exc:
            print("serial open %s: %s" % (path, exc))
            continue
        deadline = time.time() + seconds
        while time.time() < deadline:
            try:
                chunk = handle.read(256)
            except BlockingIOError:
                chunk = b""
            if chunk:
                text += chunk.decode("utf-8", "replace")
            else:
                time.sleep(0.05)
        handle.close()
        if "TARGET node" in text:
            break
    return text


def main():
    tool = pico_paths.picotool()
    if not tool or not os.path.isfile(UF2):
        print("TARGET firmware missing. Run scripts/build_firmware.sh first.")
        return 1
    env = pico_paths.with_tool_path(os.environ.copy())
    probe = subprocess.run([tool, "info"], env=env, capture_output=True, text=True)
    sys.stdout.write(probe.stdout)
    sys.stderr.write(probe.stderr)
    seen = probe.stdout + probe.stderr
    if probe.returncode != 0 and "RP2350" not in seen:
        print("TARGET hardware absent. Plug in the headered Pico 2 W on micro-USB.")
        print("Do not connect the Mega or the RR-CirKits gateway.")
        return 2
    print("loading %s" % UF2)
    loaded = subprocess.call([tool, "load", "-f", "-x", UF2], env=env)
    if loaded != 0:
        return loaded
    time.sleep(2)
    text = _read_serial(20)
    sys.stdout.write(text)
    missing = [line for line in NEEDLES if line not in text]
    if missing:
        print("TARGET serial missing: %s" % ", ".join(missing))
        return 1
    if "TARGET pin map rejected" in text or "TARGET cyw43 init failed" in text:
        print("TARGET self-check failed")
        return 1
    if "TARGET ip " in text and "TARGET gridconnect listen 12021" in text:
        print("TARGET wifi and GridConnect listener are up")
    elif "TARGET wifi secret missing" in text:
        print("TARGET booted. Wi-Fi join waits on local/wifi_psk_wrap.inc")
    print("TARGET self-check passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
