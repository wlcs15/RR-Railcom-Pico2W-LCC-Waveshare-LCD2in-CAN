#!/usr/bin/env python3
"""Flash blink.uf2 to the Pico 2 W on the main micro-USB port. No SWD probe."""
from __future__ import print_function

import os
import subprocess
import sys

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
UF2 = os.path.join(ROOT, "build", "firmware", "pico2_w", "blink.uf2")


def main():
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
        print("Plug in the headered Pico 2 W. Hold BOOTSEL if the chip is blank, then rerun.")
        print("Do not connect the three-pin SWD header.")
        return info
    print("loading %s" % UF2)
    return subprocess.call([tool, "load", "-f", "-x", UF2], env=env)


if __name__ == "__main__":
    sys.exit(main())
