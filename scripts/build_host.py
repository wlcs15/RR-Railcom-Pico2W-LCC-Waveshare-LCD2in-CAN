#!/usr/bin/env python3
"""Configure and build host Unity tests."""
from __future__ import print_function

import sys

import host_cmake


def main():
    env = host_cmake.cmake_configure(host_cmake.HOST_BUILD, coverage=False)
    host_cmake.cmake_build(host_cmake.HOST_BUILD, env)
    print("tests: %s" % host_cmake.tests_exe(host_cmake.HOST_BUILD))
    return 0


if __name__ == "__main__":
    sys.exit(main())
