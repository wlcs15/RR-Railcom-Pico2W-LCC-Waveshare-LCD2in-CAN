#!/usr/bin/env python3
"""Configure and build the host Unity tests with Clang."""
from __future__ import print_function

import os
import subprocess
import sys

import pico_paths

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
HOST_SOURCE = os.path.join(ROOT, "host")
HOST_BUILD = os.path.join(ROOT, "build", "host")
COVERAGE_BUILD = os.path.join(ROOT, "build", "host-coverage")


def _require(name):
    found = pico_paths.which(name)
    if not found:
        raise SystemExit("missing %s" % name)
    return found


def _forward(path):
    return path.replace("\\", "/")


def _rc_compiler():
    """Windows resource compiler. CMake treats backslashes in this path as escapes."""
    if os.name != "nt":
        return ""
    kits = []
    for key, default in (
        ("ProgramFiles(x86)", r"C:\Program Files (x86)"),
        ("ProgramFiles", r"C:\Program Files"),
    ):
        root = os.path.join(os.environ.get(key, default), "Windows Kits", "10", "bin")
        if not os.path.isdir(root):
            continue
        for name in os.listdir(root):
            candidate = os.path.join(root, name, "x64", "rc.exe")
            if os.path.isfile(candidate):
                kits.append(candidate)
    if kits:
        kits.sort(reverse=True)
        return _forward(kits[0])
    llvm_rc = os.path.join(os.environ.get("ProgramFiles", r"C:\Program Files"), "LLVM", "bin", "llvm-rc.exe")
    if os.path.isfile(llvm_rc):
        return _forward(llvm_rc)
    return ""


def tests_exe(build):
    name = "pin_map_tests.exe" if os.name == "nt" else "pin_map_tests"
    return os.path.join(build, name)


def cmake_configure(build, coverage):
    cmake = _require("cmake")
    clang = _require("clang")
    ninja = pico_paths.ninja()
    if not ninja:
        raise SystemExit("missing ninja")
    env = pico_paths.with_tool_path(os.environ.copy())
    cmd = [
        cmake,
        "-S",
        HOST_SOURCE,
        "-B",
        build,
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DCMAKE_C_COMPILER=%s" % clang,
        "-DRR_ENABLE_COVERAGE=%s" % ("ON" if coverage else "OFF"),
    ]
    rc = _rc_compiler()
    if rc:
        cmd.append("-DCMAKE_RC_COMPILER=%s" % rc)
    subprocess.check_call(cmd, cwd=ROOT, env=env)
    return env


def cmake_build(build, env, target=None):
    cmake = _require("cmake")
    cmd = [cmake, "--build", build]
    if target:
        cmd.extend(["--target", target])
    subprocess.check_call(cmd, cwd=ROOT, env=env)
