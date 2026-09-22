#!/usr/bin/env python3
"""Run the host pin-map Unity tests."""
from __future__ import print_function

import subprocess
import sys

import host_cmake


def main():
    env = host_cmake.cmake_configure(host_cmake.HOST_BUILD, coverage=False)
    host_cmake.cmake_build(host_cmake.HOST_BUILD, env)
    return subprocess.call([host_cmake.tests_exe(host_cmake.HOST_BUILD)])


if __name__ == "__main__":
    sys.exit(main())
