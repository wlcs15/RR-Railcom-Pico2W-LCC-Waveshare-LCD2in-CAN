#!/usr/bin/env python3
"""Report whether this tree can build and run its host checks. Does not install anything."""
from __future__ import print_function

import os
import sys

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
MISSING = {"req": 0}


def ok(name, detail):
    print("  OK       %-18s %s" % (name, detail))


def fail(name, detail):
    print("  MISSING  %-18s %s" % (name, detail))
    MISSING["req"] = 1


def main():
    print("Required tools check  repo: %s" % ROOT)
    print("")
    print("=== Host Unity + quality (required) ===")
    for name in ("git", "cmake", "clang", "python"):
        found = pico_paths.which(name) or pico_paths.which(name + "3" if name == "python" else name)
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
    if MISSING["req"]:
        print("RESULT: missing required tools")
        return 1
    print("RESULT: required tools present")
    return 0


if __name__ == "__main__":
    sys.exit(main())
