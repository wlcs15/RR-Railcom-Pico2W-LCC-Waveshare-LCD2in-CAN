#!/usr/bin/env python3
"""Locate Pico SDK 2.2 tools without requiring them on PATH."""
from __future__ import print_function

import os
import shutil


def home():
    return os.path.expanduser("~")


def pico_root():
    return os.path.join(home(), ".pico-sdk")


def _exe(path):
    if os.name == "nt" and not path.lower().endswith(".exe"):
        candidate = path + ".exe"
        if os.path.isfile(candidate):
            return candidate
    return path


def which(name):
    found = shutil.which(name)
    if found:
        return found
    return None


def sdk_path():
    env = os.environ.get("PICO_SDK_PATH", "")
    if env and os.path.isfile(os.path.join(env, "pico_sdk_init.cmake")):
        return env
    default = os.path.join(pico_root(), "sdk", "2.2.0")
    if os.path.isfile(os.path.join(default, "pico_sdk_init.cmake")):
        return default
    return ""


def toolchain_path():
    env = os.environ.get("PICO_TOOLCHAIN_PATH", "")
    gcc = _exe(os.path.join(env, "bin", "arm-none-eabi-gcc")) if env else ""
    if gcc and os.path.isfile(gcc):
        return env
    default = os.path.join(pico_root(), "toolchain", "14_2_Rel1")
    gcc = _exe(os.path.join(default, "bin", "arm-none-eabi-gcc"))
    if os.path.isfile(gcc):
        return default
    return ""


def arm_gcc():
    on_path = which("arm-none-eabi-gcc")
    if on_path:
        return on_path
    root = toolchain_path()
    if not root:
        return ""
    gcc = _exe(os.path.join(root, "bin", "arm-none-eabi-gcc"))
    if os.path.isfile(gcc):
        return gcc
    return ""


def ninja():
    found = which("ninja")
    if found:
        return found
    candidate = _exe(os.path.join(pico_root(), "ninja", "v1.12.1", "ninja"))
    if os.path.isfile(candidate):
        return candidate
    return ""


def picotool():
    found = which("picotool")
    if found:
        return found
    candidate = _exe(os.path.join(pico_root(), "picotool", "2.2.0-a4", "picotool", "picotool"))
    if os.path.isfile(candidate):
        return candidate
    return ""


def with_tool_path(env):
    """Return env with ninja and the ARM toolchain visible."""
    out = dict(env)
    prefixes = []
    ninja_bin = ninja()
    if ninja_bin:
        prefixes.append(os.path.dirname(ninja_bin))
    gcc = arm_gcc()
    if gcc:
        prefixes.append(os.path.dirname(gcc))
    tool = picotool()
    if tool:
        prefixes.append(os.path.dirname(tool))
    if prefixes:
        out["PATH"] = os.pathsep.join(prefixes + [out.get("PATH", "")])
    sdk = sdk_path()
    if sdk:
        out["PICO_SDK_PATH"] = sdk
    chain = toolchain_path()
    if chain:
        out["PICO_TOOLCHAIN_PATH"] = chain
    return out
