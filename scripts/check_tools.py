#!/usr/bin/env python3
"""Report whether this tree can build and run its host checks. Does not install anything."""
from __future__ import print_function

import os
import sys

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MISSING = {"req": 0}
WARNED = {"opt": 0}

# Prefer workspace emulator wrappers when present (GrokBot / local CI).
_WORKSPACE_BIN = "/workspace/tools/bin"


def ok(name, detail):
    print("  OK       %-18s %s" % (name, detail))


def fail(name, detail):
    print("  MISSING  %-18s %s" % (name, detail))
    MISSING["req"] = 1


def warn(name, detail):
    print("  WARN     %-18s %s" % (name, detail))
    WARNED["opt"] = 1


def _which(name):
    """Resolve a tool: PATH first, then /workspace/tools/bin if that directory exists."""
    found = pico_paths.which(name)
    if found:
        return found
    if os.path.isdir(_WORKSPACE_BIN):
        candidate = os.path.join(_WORKSPACE_BIN, name)
        if os.name == "nt":
            for ext in (".exe", ".cmd", ".bat", ""):
                c = candidate + ext
                if os.path.isfile(c) and os.access(c, os.X_OK):
                    return c
        elif os.path.isfile(candidate) and os.access(candidate, os.X_OK):
            return candidate
    return None


def main():
    # If workspace tools/bin exists, put it first so which() matches CI layout.
    if os.path.isdir(_WORKSPACE_BIN):
        os.environ["PATH"] = _WORKSPACE_BIN + os.pathsep + os.environ.get("PATH", "")

    print("Required tools check  repo: %s" % ROOT)
    print("")
    print("=== Host Unity + quality (required) ===")
    for name in ("git", "cmake", "clang", "python"):
        found = pico_paths.which(name) or pico_paths.which(
            name + "3" if name == "python" else name
        )
        if name == "python" and not found:
            found = pico_paths.which("python3")
        if found:
            ok(name, found)
        else:
            fail(name, "install %s" % name)
    ninja = pico_paths.ninja()
    if ninja:
        ok("ninja", ninja)
    else:
        fail("ninja", "Ninja, or ~/.pico-sdk/ninja/v1.12.1")
    unity = os.path.join(ROOT, "third_party", "Unity", "src", "unity.c")
    if os.path.isfile(unity):
        ok("Unity", unity)
    else:
        fail("Unity", unity)
    try:
        import lizard  # noqa: F401

        ok("lizard", "import lizard")
    except ImportError:
        fail("lizard", "pip install lizard")
    if pico_paths.which("llvm-cov") and pico_paths.which("llvm-profdata"):
        ok("llvm-cov", "host coverage")
    else:
        fail("llvm-cov", "LLVM llvm-cov and llvm-profdata")

    print("")
    print("=== Pico firmware (required) ===")
    sdk = pico_paths.sdk_path()
    if sdk:
        ok("PicoSDK", sdk)
    else:
        fail("PicoSDK", "%s/.pico-sdk/sdk/2.2.0" % pico_paths.home())
    gcc = pico_paths.arm_gcc()
    if gcc:
        ok("arm-gcc", gcc)
    else:
        fail("arm-gcc", "toolchain 14_2_Rel1")
    tool = pico_paths.picotool()
    if tool:
        ok("picotool", tool)
    else:
        fail("picotool", "picotool 2.2.0-a4")

    print("")
    print("=== On-target emulators (required) ===")
    print("  (skip Espressif QEMU / simavr — other CI bots)")
    qemu_arm = _which("qemu-system-arm")
    if qemu_arm:
        ok("qemu-system-arm", qemu_arm)
    else:
        fail(
            "qemu-system-arm",
            "install qemu-system-arm (or put /workspace/tools/bin on PATH)",
        )
    renode = _which("renode")
    if renode:
        ok("renode", renode)
    else:
        fail("renode", "install Renode (or put /workspace/tools/bin on PATH)")
    qemu_a64 = _which("qemu-system-aarch64")
    if qemu_a64:
        ok("qemu-system-aarch64", qemu_a64 + " (optional)")
    else:
        warn(
            "qemu-system-aarch64",
            "optional companion softmmu; not required for Pico M0/M33",
        )

    print("")
    if MISSING["req"]:
        print("RESULT: missing required tools")
        return 1
    if WARNED["opt"]:
        print("RESULT: required tools present (optional WARN above)")
        return 0
    print("RESULT: required tools present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
