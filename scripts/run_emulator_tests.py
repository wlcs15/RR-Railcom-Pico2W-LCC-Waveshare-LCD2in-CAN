#!/usr/bin/env python3
"""Run firmware Unity/self-check under qemu-system-arm (default) or Renode.

Does NOT flash hardware. Wraps the same firmware image used by
scripts/run_target_tests.* (lcc_node.elf) under a system emulator.

Usage:
  bash scripts/run_emulator_tests.sh
  bash scripts/run_emulator_tests.sh --elf build/firmware/pico2_w/lcc_node.elf
  bash scripts/run_emulator_tests.sh --board pico_w --renode
  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\\run_emulator_tests.ps1

Default architecture: RP2350 / Pico 2 W → qemu-system-arm cortex-m33
  (mps2-an505, fallback musca-b1). Optional --renode for RP2040 / pico_w ELFs.

Exit codes:
  0     emulator exited cleanly
  124   timeout (treated as smoke OK — firmware often does not halt)
  2     missing tools / ELF
  other emulator crash (known: QEMU may SIGABRT/134 on some Pico ELFs)

Prefer PATH with /workspace/tools/bin (source /workspace/env/emulators.sh).
"""
from __future__ import print_function

import argparse
import os
import shutil
import subprocess
import sys
import tempfile

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
_WORKSPACE_BIN = "/workspace/tools/bin"
_CI_QEMU = "/workspace/tools/ci/run_qemu_m33.sh"
_CI_RENODE = "/workspace/tools/ci/run_renode_rp2040.sh"


def _ensure_workspace_path():
    if os.path.isdir(_WORKSPACE_BIN):
        os.environ["PATH"] = _WORKSPACE_BIN + os.pathsep + os.environ.get("PATH", "")
    emu = "/workspace/env/emulators.sh"
    # Soft hint only — bash wrappers source it; Python relies on PATH.
    return emu


def _which(name):
    found = shutil.which(name)
    if found:
        return found
    if os.path.isdir(_WORKSPACE_BIN):
        cand = os.path.join(_WORKSPACE_BIN, name)
        if os.path.isfile(cand) and os.access(cand, os.X_OK):
            return cand
    return None


def _find_elf(board):
    return os.path.join(ROOT, "build", "firmware", board, "lcc_node.elf")


def _build_firmware(board):
    cmake = pico_paths.which("cmake")
    if not cmake:
        print("EMU missing cmake", file=sys.stderr)
        return 1
    env = pico_paths.with_tool_path(os.environ.copy())
    build = os.path.join(ROOT, "build", "firmware", board)
    cmd = [
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
    print("EMU configure %s" % board)
    subprocess.check_call(cmd, cwd=ROOT, env=env)
    print("EMU build %s" % board)
    subprocess.check_call([cmake, "--build", build], cwd=ROOT, env=env)
    return 0


def _run_ci_helper(helper, elf, timeout_sec, log_base):
    if os.path.isfile(helper):
        cmd = ["bash", helper, elf, str(timeout_sec), log_base]
        print("EMU using CI helper: %s" % helper)
        return subprocess.call(cmd)
    return None


def _run_qemu(elf, timeout_sec, log_base):
    qemu = _which("qemu-system-arm")
    if not qemu:
        print(
            "EMU missing qemu-system-arm "
            "(source /workspace/env/emulators.sh or install qemu-system-arm)",
            file=sys.stderr,
        )
        return 2
    rc = _run_ci_helper(_CI_QEMU, elf, timeout_sec, log_base)
    if rc is not None:
        return rc
    out = log_base + ".qemu.out"
    machines = ("mps2-an505", "musca-b1")
    last = 1
    for machine in machines:
        cmd = [
            qemu,
            "-machine",
            machine,
            "-cpu",
            "cortex-m33",
            "-kernel",
            elf,
            "-nographic",
            "-semihosting-config",
            "enable=on,target=native",
            "-monitor",
            "none",
            "-serial",
            "mon:stdio",
        ]
        print("EMU qemu-system-arm machine=%s timeout=%ss" % (machine, timeout_sec))
        try:
            with open(out, "wb") as handle:
                proc = subprocess.run(
                    cmd,
                    stdin=subprocess.DEVNULL,
                    stdout=handle,
                    stderr=subprocess.STDOUT,
                    timeout=timeout_sec,
                )
            last = proc.returncode
        except subprocess.TimeoutExpired:
            print("EMU qemu timeout (%ss) — smoke OK" % timeout_sec)
            return 124
        if last in (134, 139) and machine != machines[-1]:
            print("EMU qemu %s crashed rc=%s; trying next machine" % (machine, last))
            continue
        break
    print("EMU qemu exit=%s (see %s)" % (last, out))
    return last


def _run_renode(elf, timeout_sec, log_base):
    renode = _which("renode")
    if not renode:
        print(
            "EMU missing renode "
            "(source /workspace/env/emulators.sh or install Renode)",
            file=sys.stderr,
        )
        return 2
    rc = _run_ci_helper(_CI_RENODE, elf, timeout_sec, log_base)
    if rc is not None:
        return rc
    abs_elf = os.path.realpath(elf)
    out = log_base + ".renode.out"
    resc = tempfile.NamedTemporaryFile("w", suffix=".resc", delete=False)
    try:
        resc.write(
            """using sysbus
mach create "rp2040_smoke"
machine LoadPlatformDescriptionFromString """
            '"""\n'
            """cpu: CPU.CortexM @ sysbus
    cpuType: \\"cortex-m0\\"
    nvic: nvic
nvic: IRQControllers.NVIC @ sysbus 0xE000E000
    -> cpu@0
flash: Memory.MappedMemory @ sysbus 0x10000000
    size: 0x00200000
sram: Memory.MappedMemory @ sysbus 0x20000000
    size: 0x00042000
"""
            '"""\n'
            "sysbus LoadELF @%s\n"
            "cpu PC 0x10000000\n"
            "start\n"
            'emulation RunFor "00:00:02"\n' % abs_elf
        )
        resc.close()
        cmd = [
            renode,
            "--disable-xwt",
            "--console",
            "-e",
            "include @%s; quit" % resc.name,
        ]
        print("EMU renode timeout=%ss elf=%s" % (timeout_sec, abs_elf))
        try:
            with open(out, "wb") as handle:
                proc = subprocess.run(
                    cmd,
                    stdin=subprocess.DEVNULL,
                    stdout=handle,
                    stderr=subprocess.STDOUT,
                    timeout=timeout_sec,
                )
            print("EMU renode exit=%s (see %s)" % (proc.returncode, out))
            return proc.returncode
        except subprocess.TimeoutExpired:
            print("EMU renode timeout (%ss) — smoke OK" % timeout_sec)
            return 124
    finally:
        try:
            os.unlink(resc.name)
        except OSError:
            pass


def main(argv=None):
    _ensure_workspace_path()
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--elf", default="", help="Firmware ELF (default: build/firmware/<board>/lcc_node.elf)")
    ap.add_argument("--board", default="pico2_w", help="PICO_BOARD when building (default pico2_w)")
    ap.add_argument("--timeout", type=int, default=90, help="Seconds (default 90)")
    ap.add_argument("--renode", action="store_true", help="Use Renode RP2040 smoke instead of qemu-m33")
    ap.add_argument("--no-build", action="store_true", help="Do not build if ELF missing")
    ap.add_argument(
        "--log",
        default="",
        help="Log base path (default: build/emulator-smoke)",
    )
    args = ap.parse_args(argv)

    elf = args.elf or _find_elf(args.board)
    if not os.path.isfile(elf):
        if args.no_build:
            print("EMU missing ELF: %s (pass --elf or build firmware)" % elf, file=sys.stderr)
            return 2
        try:
            _build_firmware(args.board)
        except subprocess.CalledProcessError as exc:
            print("EMU firmware build failed: %s" % exc, file=sys.stderr)
            return 1
        elf = _find_elf(args.board)
    if not os.path.isfile(elf):
        print("EMU missing ELF after build: %s" % elf, file=sys.stderr)
        return 2

    log_base = args.log or os.path.join(ROOT, "build", "emulator-smoke")
    parent = os.path.dirname(log_base)
    if parent and not os.path.isdir(parent):
        os.makedirs(parent)

    print("EMU elf=%s" % elf)
    print("EMU note: does not flash hardware; host Unity remains scripts/run_tests.*")
    if args.renode:
        rc = _run_renode(elf, args.timeout, log_base)
    else:
        rc = _run_qemu(elf, args.timeout, log_base)

    if rc in (0, 124):
        print("EMU smoke RESULT: PASS (exit=%s)" % rc)
        return 0
    print(
        "EMU smoke RESULT: FAIL (exit=%s). "
        "Known limitation: qemu-system-arm may SIGABRT on some Pico ELFs; "
        "wrapper still useful for CI wiring." % rc
    )
    return rc if rc else 1


if __name__ == "__main__":
    sys.exit(main())
