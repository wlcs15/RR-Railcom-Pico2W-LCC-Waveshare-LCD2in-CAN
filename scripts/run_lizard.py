#!/usr/bin/env python3
"""Fail if any function in src/ or tests/ has cyclomatic complexity above 10."""
from __future__ import print_function

import os
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
LIMIT = 10
MODULES = ("src", "tests")


def main():
    try:
        import lizard
    except ImportError:
        print("MISSING lizard (pip install lizard)")
        return 1
    failed = 0
    print("Cyclomatic complexity (limit %d)" % LIMIT)
    print("%-28s %5s %5s %6s" % ("File", "NLOC", "Funcs", "MaxCCN"))
    for module in MODULES:
        folder = os.path.join(ROOT, module)
        for dirpath, dirnames, filenames in os.walk(folder):
            dirnames[:] = [name for name in dirnames if name not in ("build", "third_party")]
            for filename in sorted(filenames):
                if not filename.endswith((".c", ".h")):
                    continue
                path = os.path.join(dirpath, filename)
                info = lizard.analyze_file(path)
                max_ccn = 0
                for func in info.function_list:
                    if func.cyclomatic_complexity > max_ccn:
                        max_ccn = func.cyclomatic_complexity
                    if func.cyclomatic_complexity > LIMIT:
                        failed = 1
                        print("FAIL %s %s CCN %d" % (path, func.name, func.cyclomatic_complexity))
                rel = os.path.relpath(path, ROOT)
                print("%-28s %5d %5d %6d" % (rel, info.nloc, len(info.function_list), max_ccn))
    if failed:
        print("RESULT: a function exceeds CCN %d" % LIMIT)
        return 1
    print("RESULT: no function exceeds CCN %d" % LIMIT)
    return 0


if __name__ == "__main__":
    sys.exit(main())
