#!/usr/bin/env python3
"""Small platform-safety audit for the public Slammin Tuner source tree.

This is intentionally narrow. It catches accidental unguarded Win32 API use in
shared plugin/editor files before a macOS AU build reaches the compiler.
"""

from __future__ import annotations

import pathlib
import re
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE_ROOT = ROOT / "src" / "strobe_tuner"
WINDOWS_ONLY_PATTERNS = (
    re.compile(r"\bHWND\b"),
    re.compile(r"\bHHOOK\b"),
    re.compile(r"\bWNDPROC\b"),
    re.compile(r"\bLRESULT\b"),
    re.compile(r"\bWPARAM\b"),
    re.compile(r"\bLPARAM\b"),
    re.compile(r"\bCreateWindowExW\b"),
    re.compile(r"\bSetWindowsHookExW\b"),
    re.compile(r"\bwindows\.h\b", re.IGNORECASE),
)


def main() -> int:
    failures: list[str] = []

    for path in sorted(SOURCE_ROOT.glob("*.[ch]pp")) + sorted(SOURCE_ROOT.glob("*.h")):
        windows_guard_depth = 0
        lines = path.read_text(encoding="utf-8").splitlines()

        for line_number, line in enumerate(lines, start=1):
            stripped = line.strip()

            if stripped.startswith("#if") and "JUCE_WINDOWS" in stripped:
                windows_guard_depth += 1
            elif stripped.startswith("#endif") and windows_guard_depth > 0:
                windows_guard_depth -= 1

            if windows_guard_depth > 0:
                continue

            for pattern in WINDOWS_ONLY_PATTERNS:
                if pattern.search(line):
                    rel = path.relative_to(ROOT)
                    failures.append(f"{rel}:{line_number}: unguarded Windows-only token: {line.strip()}")

    if failures:
        print("Platform guard audit failed:")
        print("\n".join(failures))
        return 1

    print("Platform guard audit passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

