#!/usr/bin/env python3
"""Host llvm-cov report for src/pin_map.c."""
from __future__ import print_function

import sys

import host_cmake


def main():
    env = host_cmake.cmake_configure(host_cmake.COVERAGE_BUILD, coverage=True)
    host_cmake.cmake_build(host_cmake.COVERAGE_BUILD, env, target="coverage")
    print("HTML report: %s" % os_path())
    return 0


def os_path():
    import os

    return os.path.join(host_cmake.COVERAGE_BUILD, "coverage", "index.html")


if __name__ == "__main__":
    sys.exit(main())
