#!/usr/bin/env python3
"""Run the QML examples in check mode and lint their QML against the types they publish.

    python3 qt/examples/qml-check.py build

`--check` loads the headless `Check.qml` rather than the window `Main.qml`, drives
every published shape from QML and quits. Each example must exit 0, print its one
summary line on stdout, and print nothing on stderr: a QML resolution failure
leaves the exit code at 0, so an empty stderr is the assertion that matters. `qmllint` then reads the `.qmltypes`
`qmltyperegistrar` generated from the reflex metatypes document and checks every
name the QML uses against it, which is what proves the metadata rather than the
plumbing. `QMLLINT` in the environment overrides the Qt 6 path it defaults to,
since `qmllint` on `PATH` is Qt 5 on some distributions.

Running the examples is the only place a `QQmlEngine` builds a property cache
over a reflex class. A shape the engine rejects fails object creation without
printing anything, so `run_example` reads the exit code rather than the output.

Not a doctest: the examples need Qt Qml and its tools, which the suite must not
depend on being installed.
"""

import argparse
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile

QMLLINT = pathlib.Path(os.environ.get("QMLLINT", "/usr/lib/qt6/bin/qmllint"))
HERE = pathlib.Path(__file__).resolve().parent

EXAMPLES = {
    "clock": (
        "Reflex.Clock",
        "span [-5, 10] width 15 shade 2 ticks 10 observed 11 note reflex clock",
    ),
}


def run_example(build, name, expected):
    program = build / "qt" / f"reflex-qt-{name}"
    result = subprocess.run(
        [program, "--check"],
        capture_output=True,
        text=True,
        timeout=60,
        env=os.environ | {"QT_QPA_PLATFORM": "offscreen", "LC_ALL": "C.UTF-8"},
    )
    problems = []
    if result.returncode != 0:
        problems.append(f"exit code {result.returncode}")
    if result.stderr:
        problems.append(f"stderr: {result.stderr.strip()}")
    if result.stdout.strip() != expected:
        problems.append(f"stdout: {result.stdout.strip()!r}, expected {expected!r}")
    return problems


def run_qmllint(build, name, uri, staging):
    module = staging.joinpath(*uri.split("."))
    module.mkdir(parents=True)
    generated = build / f"qt.reflex-qt-{name}-types"
    shutil.copy(generated / "qmldir", module)
    for types in generated.glob("*.qmltypes"):
        shutil.copy(types, module)
    for qml in (HERE / name).glob("*.qml"):
        shutil.copy(qml, module)

    result = subprocess.run(
        [QMLLINT, "-I", staging, *sorted(module.glob("*.qml"))],
        capture_output=True,
        text=True,
        timeout=60,
    )
    problems = [line for line in (result.stdout + result.stderr).splitlines() if line]
    if result.returncode != 0 and not problems:
        problems.append(f"qmllint exited {result.returncode} without saying why")
    return problems


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=pathlib.Path, help="the build directory")
    args = parser.parse_args()

    if not QMLLINT.is_file():
        print(f"{QMLLINT} is not there; set QMLLINT to a Qt 6 qmllint", file=sys.stderr)
        return 2

    failed = False
    with tempfile.TemporaryDirectory() as raw:
        staging = pathlib.Path(raw)
        for name, (uri, expected) in EXAMPLES.items():
            problems = run_example(args.build, name, expected)
            problems += run_qmllint(args.build, name, uri, staging / name)
            failed = failed or bool(problems)
            print(f"=== {name} ===")
            print("\n".join(problems) if problems else "clean")

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
