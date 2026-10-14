#!/usr/bin/env python3
"""Shared emulator smoke runner (Renode default, --qemu experimental). Does NOT flash.

One identical copy lives in each repo that uses it:
  RR-Railcom-Pico2W-LCC-Waveshare-LCD2in-CAN : scripts/run_emulator_tests.*
  Waveshare_3.5_inch_Touch_Display_Module    : Pico-ResTouch-LCD-X_X_Code/C/scripts/run_emulator_tests.*

Profile is auto-detected from the project tree (override with --profile):
  firmware  : LCC node tree (scripts/build_firmware.py present).
              ELF build/firmware/<board>/lcc_node.elf, default board pico2_w.
  on_target : Waveshare tree (tests/on_target present).
              ELF build/on_target/on_target_tests.elf, else build/main.elf; board pico_w.

Usage:
  bash scripts/run_emulator_tests.sh [--elf PATH] [--board B] [--timeout SEC] [--qemu] [--no-build]
  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\\run_emulator_tests.ps1

Emulator helpers are the repo copies under <git root>/scripts/ci/
(run_renode_rp2040.sh, run_qemu_m33.sh); /workspace/tools/ci is a fallback.

Stock qemu-system-arm has NO pico/rp2040/rp2350 machine. A Pico ELF on
mps2-an505/musca-b1 HardFaults then QEMU aborts (Lockup, exit 134), so --qemu
is experimental only.

Exit codes: 0 PASS (emulator exit 0 or timeout 124), 2 missing tools/ELF,
other = emulator failure.
"""
from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, ".."))
_WORKSPACE_BIN = "/workspace/tools/bin"


def _git_root():
    try:
        out = subprocess.check_output(
            ["git", "-C", ROOT, "rev-parse", "--show-toplevel"], stderr=subprocess.DEVNULL
        )
        return out.decode().strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def _helper(name):
    dirs = [os.path.join(ROOT, "scripts", "ci")]
    top = _git_root()
    if top:
        dirs.append(os.path.join(top, "scripts", "ci"))
    dirs.append("/workspace/tools/ci")
    for d in dirs:
        cand = os.path.join(d, name)
        if os.path.isfile(cand):
            return cand
    return None


def _which(name):
    found = shutil.which(name)
    if found:
        return found
    cand = os.path.join(_WORKSPACE_BIN, name)
    if os.path.isfile(cand) and os.access(cand, os.X_OK):
        return cand
    return None


def _detect_profile():
    if os.path.isfile(os.path.join(ROOT, "scripts", "build_firmware.py")):
        return "firmware"
    if os.path.isdir(os.path.join(ROOT, "tests", "on_target")):
        return "on_target"
    return "firmware"


def _default_elf(profile, board):
    if profile == "firmware":
        return os.path.join(ROOT, "build", "firmware", board, "lcc_node.elf")
    for path in (
        os.path.join(ROOT, "build", "on_target", "on_target_tests.elf"),
        os.path.join(ROOT, "build", "main.elf"),
    ):
        if os.path.isfile(path):
            return path
    return os.path.join(ROOT, "build", "on_target", "on_target_tests.elf")


def _sdk_env():
    env = os.environ.copy()
    sys.path.insert(0, HERE)
    try:
        import pico_paths  # per-repo helper; API differs between repos
    except ImportError:
        pico_paths = None
    if pico_paths is not None:
        for fn, key in (("sdk_path", "PICO_SDK_PATH"), ("sdk_root", "PICO_SDK_PATH"),
                        ("toolchain_path", "PICO_TOOLCHAIN_PATH"),
                        ("toolchain_root", "PICO_TOOLCHAIN_PATH")):
            if key not in env and hasattr(pico_paths, fn):
                try:
                    val = getattr(pico_paths, fn)()
                    if val:
                        env[key] = val
                except Exception:  # pylint: disable=broad-except
                    pass
        if hasattr(pico_paths, "with_tool_path"):
            env = pico_paths.with_tool_path(env)
    if os.path.isdir(_WORKSPACE_BIN):
        env["PATH"] = _WORKSPACE_BIN + os.pathsep + env.get("PATH", "")
    return env


def _build(profile, board):
    cmake = _which("cmake")
    if not cmake:
        print("EMU missing cmake", file=sys.stderr)
        return 2
    env = _sdk_env()
    gen = ["-G", "Ninja"] if _which("ninja") else []
    if profile == "firmware":
        src = ROOT
        build = os.path.join(ROOT, "build", "firmware", board)
        extra = ["-DPICO_BOARD=%s" % board, "-DLCC_TRANSPORT=WIFI",
                 "-DLCD_PANEL=NONE", "-DRR_KEY0_GPIO=15"]
    else:
        src = os.path.join(ROOT, "tests", "on_target")
        build = os.path.join(ROOT, "build", "on_target")
        extra = ["-DPICO_BOARD=%s" % board, "-DCMAKE_BUILD_TYPE=Release"]
    print("EMU configure profile=%s board=%s" % (profile, board))
    subprocess.check_call([cmake, "-S", src, "-B", build] + gen + extra, cwd=ROOT, env=env)
    subprocess.check_call([cmake, "--build", build], cwd=ROOT, env=env)
    return 0


def _run(kind, elf, timeout_sec, log_base):
    tool, helper = (("qemu-system-arm", "run_qemu_m33.sh") if kind == "qemu"
                    else ("renode", "run_renode_rp2040.sh"))
    if not _which(tool):
        print("EMU missing %s (source /workspace/env/emulators.sh)" % tool, file=sys.stderr)
        return 2
    path = _helper(helper)
    if not path:
        print("EMU missing helper scripts/ci/%s" % helper, file=sys.stderr)
        return 2
    print("EMU using helper: %s" % path)
    return subprocess.call(["bash", path, elf, str(timeout_sec), log_base])


def main(argv=None):
    if os.path.isdir(_WORKSPACE_BIN):
        os.environ["PATH"] = _WORKSPACE_BIN + os.pathsep + os.environ.get("PATH", "")
    ap = argparse.ArgumentParser(description="Renode (default) / QEMU (experimental) smoke")
    ap.add_argument("--profile", choices=("firmware", "on_target"), default="")
    ap.add_argument("--elf", default="")
    ap.add_argument("--board", default="", help="PICO_BOARD (firmware: pico2_w, on_target: pico_w)")
    ap.add_argument("--timeout", type=int, default=90)
    ap.add_argument("--qemu", action="store_true", help="Experimental; Pico ELF usually exits 134")
    ap.add_argument("--renode", action="store_true", help="Force Renode (default)")
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--log", default="", help="Log base (default build/emulator-smoke)")
    args = ap.parse_args(argv)

    profile = args.profile or _detect_profile()
    board = args.board or ("pico2_w" if profile == "firmware" else "pico_w")
    elf = args.elf or _default_elf(profile, board)
    if not os.path.isfile(elf):
        if args.no_build:
            print("EMU missing ELF: %s" % elf, file=sys.stderr)
            return 2
        try:
            rc = _build(profile, board)
        except subprocess.CalledProcessError as exc:
            print("EMU build failed: %s" % exc, file=sys.stderr)
            return 1
        if rc:
            return rc
        elf = args.elf or _default_elf(profile, board)
    if not os.path.isfile(elf):
        print("EMU missing ELF after build: %s" % elf, file=sys.stderr)
        return 2

    log_base = args.log or os.path.join(ROOT, "build", "emulator-smoke")
    if os.path.dirname(log_base):
        os.makedirs(os.path.dirname(log_base), exist_ok=True)
    print("EMU profile=%s elf=%s (does not flash hardware)" % (profile, elf))
    kind = "qemu" if (args.qemu and not args.renode) else "renode"
    if kind == "qemu":
        print("EMU WARNING: --qemu is experimental; no Pico machine in stock QEMU (expect 134).")
    rc = _run(kind, elf, args.timeout, log_base)
    if rc in (0, 124):
        print("EMU smoke RESULT: PASS (exit=%s)" % rc)
        return 0
    print("EMU smoke RESULT: FAIL (exit=%s)" % rc)
    return rc if rc else 1


if __name__ == "__main__":
    sys.exit(main())
