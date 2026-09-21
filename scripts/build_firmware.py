#!/usr/bin/env python3
"""Build the USB blink image for pico2_w, pico2, and pico_w."""
from __future__ import print_function

import os
import subprocess
import sys

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BOARDS = ("pico2_w", "pico2", "pico_w")


def main():
    cmake = pico_paths.which("cmake")
    if not cmake:
        print("missing cmake")
        return 1
    env = pico_paths.with_tool_path(os.environ.copy())
    for board in BOARDS:
        build = os.path.join(ROOT, "build", "firmware", board)
        configure = [
            cmake,
            "-S",
            ROOT,
            "-B",
            build,
            "-G",
            "Ninja",
            "-DPICO_BOARD=%s" % board,
            "-DLCC_TRANSPORT=WIFI",
            "-DLCD_PANEL=NONE",
            "-DRR_KEY0_GPIO=15",
        ]
        print("configure %s" % board)
        subprocess.check_call(configure, cwd=ROOT, env=env)
        print("build %s" % board)
        subprocess.check_call([cmake, "--build", build], cwd=ROOT, env=env)
        uf2 = os.path.join(build, "blink.uf2")
        if not os.path.isfile(uf2):
            print("missing %s" % uf2)
            return 1
        print("uf2: %s" % uf2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
