#!/usr/bin/env python3
"""Build the LCC node firmware.

Board hint: argv[1] or PICO_BOARD (one of pico, pico_w, pico2, pico2_w).
Transport: LCC_TRANSPORT (WIFI, CAN, or BOTH). Default here is WIFI.
LCD panel: LCD_PANEL, or the per-board default below.
If local/hub_host.h is absent, install scripts/ci_hub_host.h (dummy 192.168.1.1).
Never overwrite an existing local/hub_host.h.
"""
from __future__ import print_function

import os
import subprocess
import sys

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BOARDS = ("pico2_w", "pico2", "pico_w", "pico")
TRANSPORTS = ("WIFI", "CAN", "BOTH")


def ensure_ci_hub_host():
    dest = os.path.join(ROOT, "local", "hub_host.h")
    if os.path.isfile(dest):
        print("hub_host: keeping existing local/hub_host.h")
        return 0
    src = os.path.join(ROOT, "scripts", "ci_hub_host.h")
    if not os.path.isfile(src):
        print("missing scripts/ci_hub_host.h")
        return 1
    os.makedirs(os.path.join(ROOT, "local"), exist_ok=True)
    with open(src, "r") as infile:
        text = infile.read()
    with open(dest, "w") as outfile:
        outfile.write(text)
    print("hub_host: installed CI stub local/hub_host.h (dummy RR_HUB_HOST 192.168.1.1)")
    return 0


def selected_boards():
    hint = os.environ.get("PICO_BOARD", "").strip()
    if len(sys.argv) > 1 and sys.argv[1] and not sys.argv[1].startswith("-"):
        hint = sys.argv[1].strip()
    if not hint:
        return BOARDS
    if hint not in BOARDS:
        print("unknown PICO_BOARD %s" % hint)
        return None
    return (hint,)


def selected_transport():
    transport = os.environ.get("LCC_TRANSPORT", "WIFI").strip()
    if transport not in TRANSPORTS:
        print("unknown LCC_TRANSPORT %s" % transport)
        return None
    return transport


def lcd_panel(board):
    panel = os.environ.get("LCD_PANEL", "").strip()
    if panel:
        return panel
    return "NONE"


def main():
    if ensure_ci_hub_host() != 0:
        return 1
    boards = selected_boards()
    transport = selected_transport()
    if boards is None or transport is None:
        return 1
    cmake = pico_paths.which("cmake")
    if not cmake:
        print("missing cmake")
        return 1
    env = pico_paths.with_tool_path(os.environ.copy())
    for board in boards:
        build = os.path.join(ROOT, "build", "firmware", board)
        panel = lcd_panel(board)
        configure = [
            cmake,
            "-S",
            ROOT,
            "-B",
            build,
            "-G",
            "Ninja",
            "-DPICO_BOARD=%s" % board,
            "-DLCC_TRANSPORT=%s" % transport,
            "-DLCD_PANEL=%s" % panel,
            "-DRR_KEY0_GPIO=15",
        ]
        print("configure %s transport=%s lcd=%s" % (board, transport, panel))
        subprocess.check_call(configure, cwd=ROOT, env=env)
        print("build %s" % board)
        subprocess.check_call([cmake, "--build", build], cwd=ROOT, env=env)
        uf2 = os.path.join(build, "lcc_node.uf2")
        if not os.path.isfile(uf2):
            print("missing %s" % uf2)
            return 1
        print("uf2: %s" % uf2)
    return 0


if __name__ == "__main__":
    sys.exit(main())
