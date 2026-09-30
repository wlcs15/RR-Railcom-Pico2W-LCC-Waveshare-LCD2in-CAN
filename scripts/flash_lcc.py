#!/usr/bin/env python3
"""Flash lcc_node.uf2 to the Pico 2 W on the main micro-USB port. No SWD probe."""
from __future__ import print_function

import os
import subprocess
import sys

import pico_paths
import argparse
import time
import shutil

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UF2 = os.path.join(ROOT, "build", "firmware", "pico2_w", "lcc_node.uf2")

def wait_port(dev, timeout_s=30):
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if os.path.exists(dev):
            return True
        time.sleep(0.1)
    return False
def open_terminal(dev):
    if shutil.which("picocom"):
        os.execvp("picocom", ["picocom", "-b", "115200", "--imap", "lfcrlf", dev])
    if shutil.which("minicom"):
        os.execvp("minicom", ["minicom", "-D", dev, "-b", "115200", "-o"])
    print("neither picocom nor minicom found")
    return 1
def main():
    ap = argparse.ArgumentParser(description="Flash lcc_node.uf2 to Pico 2 W")
    ap.add_argument("-t", "--terminal", action="store_true",
                    help="after flash, wait for CDC and open picocom/minicom")
    ap.add_argument("--port", default="/dev/ttyACM0",
                    help="serial device for -t (default /dev/ttyACM0)")
    args = ap.parse_args()
    tool = pico_paths.picotool()
    if not tool:
        print("missing picotool")
        return 1
    if not os.path.isfile(UF2):
        print("missing %s — run scripts/build_firmware first" % UF2)
        return 1
    env = pico_paths.with_tool_path(os.environ.copy())
    print("picotool info (headered Pico 2 W, micro-USB only):")
    info = subprocess.call([tool, "info"], env=env)
    if info != 0:
        print("No Pico answered on USB.")
        return info
    print("loading %s" % UF2)
    rc = subprocess.call([tool, "load", "-f", "-x", UF2], env=env)
    if rc != 0:
        return rc
    if not args.terminal:
        return 0
    # -x resets; ACM drops then comes back
    print("waiting for %s ..." % args.port)
    if not wait_port(args.port, 30):
        print("timeout waiting for %s" % args.port)
        return 1
    time.sleep(0.3)  # let CDC finish enumerating
    print("opening terminal on %s" % args.port)
    return open_terminal(args.port)

if __name__ == "__main__":
    sys.exit(main())
