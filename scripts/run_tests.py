#!/usr/bin/env python3
"""Run the host pin-map Unity tests."""
from __future__ import print_function

import os
import subprocess
import sys

import host_cmake


def main():
    exe = host_cmake.tests_exe(host_cmake.HOST_BUILD)
    if not os.path.isfile(exe):
        env = host_cmake.cmake_configure(host_cmake.HOST_BUILD, coverage=False)
        host_cmake.cmake_build(host_cmake.HOST_BUILD, env)
    return subprocess.call([exe])


if __name__ == "__main__":
    sys.exit(main())
