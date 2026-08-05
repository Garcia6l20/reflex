"""A test runner small enough to have no dependencies.

pytest is not installed for the interpreter the extensions are built against,
and the build must not depend on one that is. Every test script here calls
`run(globals())`, which invokes each `test_*` function and reports.
"""

from __future__ import annotations

import sys
import traceback
from typing import Any


def run(namespace: dict[str, Any]) -> None:
    tests = [
        (name, fn)
        for name, fn in sorted(namespace.items())
        if name.startswith("test_") and callable(fn)
    ]
    failures = 0
    for name, fn in tests:
        try:
            fn()
        except Exception:
            failures += 1
            print(f"FAIL {name}", file=sys.stderr)
            traceback.print_exc()
        else:
            print(f"ok   {name}")
    if failures:
        print(f"{failures}/{len(tests)} failed", file=sys.stderr)
    sys.exit(1 if failures else 0)
