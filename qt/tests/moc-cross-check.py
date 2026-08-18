#!/usr/bin/env python3
"""Diff reflex.qt's metatypes document against real moc's for an equivalent class.

`moc-mirror.hpp` is a hand-written Q_OBJECT class; `twin` in `moc-pair.hpp` is
the same shape through reflex.qt. Real moc runs on the first, the exporter
program on the second, both documents are normalized, and the diff is what this
half of reflex.qt is verified by. qmltyperegistrar then runs on both and its two
.qmltypes are diffed the same way.

    python3 qt/tests/moc-cross-check.py build/qt/tests/reflex-qt-moc-export

Normalization drops what the two sides cannot agree on by construction: keys are
sorted, `lineNumber` / `inputFile` / `outputRevision` are removed, and the class
name is folded to CLASS on both sides so `mirror::Mode` and `twin::Mode` compare
equal. Everything else that differs is a finding.

Not a doctest: a test binary shelling out to moc would make the suite depend on
Qt's tools being installed.
"""

import argparse
import difflib
import json
import pathlib
import re
import subprocess
import sys
import tempfile

QT_LIBEXEC = pathlib.Path("/usr/lib/qt6")
HERE = pathlib.Path(__file__).resolve().parent
DROPPED = ("lineNumber", "inputFile", "outputRevision")
CLASS_NAMES = ("mirror", "twin")


def normalize(node):
    if isinstance(node, dict):
        return {k: normalize(v) for k, v in sorted(node.items()) if k not in DROPPED}
    if isinstance(node, list):
        return [normalize(v) for v in node]
    if isinstance(node, str):
        return re.sub(r"\b(%s)\b" % "|".join(CLASS_NAMES), "CLASS", node)
    return node


def unified(left, right, left_name, right_name):
    return list(difflib.unified_diff(left, right, left_name, right_name, lineterm=""))


def dump(node):
    return json.dumps(normalize(node), indent=2, sort_keys=True).splitlines()


def clean_qmltypes(text):
    kept = []
    for line in text.splitlines():
        if "lineNumber" in line or line.strip().startswith("file:"):
            continue
        kept.append(re.sub(r"\b(%s)\b" % "|".join(CLASS_NAMES), "CLASS", line))
    return kept


def run_moc(header, out_cpp):
    subprocess.run(
        [
            QT_LIBEXEC / "moc",
            "-I/usr/include/qt6",
            "-I/usr/include/qt6/QtCore",
            "--output-json",
            "-o",
            out_cpp,
            header,
        ],
        check=True,
    )
    return json.loads(out_cpp.with_suffix(".cpp.json").read_text())


def run_registrar(document, out_qmltypes, tmp):
    source = tmp / (out_qmltypes.stem + ".json")
    source.write_text(json.dumps(document if isinstance(document, list) else [document]))
    subprocess.run(
        [
            QT_LIBEXEC / "qmltyperegistrar",
            "--import-name",
            "reflex.crosscheck",
            "--major-version",
            "1",
            "--minor-version",
            "0",
            "--generate-qmltypes",
            out_qmltypes,
            "-o",
            tmp / (out_qmltypes.stem + "_reg.cpp"),
            source,
        ],
        check=True,
    )
    return out_qmltypes.read_text()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("exporter", help="the built reflex-qt-moc-export program")
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as raw:
        tmp = pathlib.Path(raw)

        moc_document = run_moc(HERE / "moc-mirror.hpp", tmp / "moc_mirror.cpp")

        reflex_json = tmp / "reflex_twin.json"
        subprocess.run([args.exporter, reflex_json], check=True)
        reflex_document = json.loads(reflex_json.read_text())

        metatypes_diff = unified(
            dump([moc_document]), dump(reflex_document), "moc(mirror)", "reflex(twin)"
        )

        qmltypes_diff = unified(
            clean_qmltypes(run_registrar(moc_document, tmp / "mirror.qmltypes", tmp)),
            clean_qmltypes(run_registrar(reflex_document, tmp / "twin.qmltypes", tmp)),
            "moc(mirror).qmltypes",
            "reflex(twin).qmltypes",
        )

    print("=== metatypes JSON ===")
    print("\n".join(metatypes_diff) if metatypes_diff else "identical")
    print()
    print("=== generated .qmltypes ===")
    print("\n".join(qmltypes_diff) if qmltypes_diff else "identical")

    return 1 if metatypes_diff or qmltypes_diff else 0


if __name__ == "__main__":
    sys.exit(main())
