#!/usr/bin/env python3
r"""Diff reflex.qt's metatypes document against real moc's for equivalent classes.

`moc-mirror.hpp` holds hand-written `Q_OBJECT` and `Q_GADGET` classes; `twin`,
`twin_qml` and `twin_gadget` in `moc-pair.hpp` are the same shapes through
reflex.qt. Real moc runs on the first, the exporter program on the second, both
documents are normalized, and the diff is what this half of reflex.qt is
verified by. qmltyperegistrar then runs on both and its two .qmltypes are diffed
the same way.

    python3 qt/tests/moc-cross-check.py --compile-dir build \
        build/qt/tests/reflex-qt-moc-export

`--compile-dir` is the directory the exporter's sources were compiled from,
which is what the exporter needs to resolve the relative path the compiler
recorded. It defaults to the working directory, which is what `pcons test`
runs the cross-check from.

Normalization drops what the two sides cannot agree on by construction: keys are
sorted, `lineNumber` / `inputFile` / `outputRevision` are removed, and the class
name is folded to CLASS on both sides so `mirror::Mode` and `twin::Mode` compare
equal. Signal parameter names go too, and that one is a real divergence rather
than a formatting one: a `signal<int>` data member carries types and no names,
so reflex cannot produce them. They are dropped before qmltyperegistrar runs, so
the generated .qmltypes are compared on everything else.

Everything else that differs is a finding.

`pcons test` runs this as `qt.moc-cross-check`, and both the build script and
this script skip rather than fail when moc, qmltyperegistrar or Qt's headers are
absent: the suite must not depend on Qt's tools being installed.
"""

import argparse
import difflib
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
import tempfile

QT_LIBEXEC = pathlib.Path("/usr/lib/qt6")
QT_HEADERS = pathlib.Path("/usr/include/qt6")
HERE = pathlib.Path(__file__).resolve().parent
DROPPED = ("lineNumber", "inputFile", "outputRevision")
CLASS_NAMES = (
    "mirror",
    "twin",
    "mirror_qml",
    "twin_qml",
    "mirror_gadget",
    "twin_gadget",
)


def is_qt6(tool):
    try:
        reported = subprocess.run(
            [tool, "--version"], capture_output=True, text=True, check=True
        ).stdout
    except (OSError, subprocess.CalledProcessError):
        return False
    return reported.split()[-1].startswith("6.")


def find_tool(name, override):
    """The Qt 6 spelling of @p name, or None.

    PATH is searched last and the version checked, because a distribution that
    ships Qt 5 alongside Qt 6 puts Qt 5's moc in /usr/bin and Qt 6's in libexec.
    """
    candidates = [pathlib.Path(override)] if override else [QT_LIBEXEC / name]
    if not override:
        found = shutil.which(name)
        if found:
            candidates.append(pathlib.Path(found))
    for candidate in candidates:
        if candidate.is_file() and is_qt6(candidate):
            return candidate
    return None


def normalize(node):
    if isinstance(node, dict):
        return {k: normalize(v) for k, v in sorted(node.items()) if k not in DROPPED}
    if isinstance(node, list):
        return [normalize(v) for v in node]
    if isinstance(node, str):
        return re.sub(r"\b(%s)\b" % "|".join(CLASS_NAMES), "CLASS", node)
    return node


def drop_signal_argument_names(documents):
    for document in documents:
        for described in document.get("classes", []):
            for signal in described.get("signals", []):
                for argument in signal.get("arguments", []):
                    argument.pop("name", None)
    return documents


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


def run_moc(moc, headers, header, out_cpp):
    subprocess.run(
        [
            moc,
            f"-I{headers}",
            f"-I{headers}/QtCore",
            f"-I{headers}/QtQmlIntegration",
            "--output-json",
            "-o",
            out_cpp,
            header,
        ],
        check=True,
    )
    return json.loads(out_cpp.with_suffix(".cpp.json").read_text())


def run_registrar(registrar, documents, out_qmltypes, tmp):
    source = tmp / (out_qmltypes.stem + ".json")
    source.write_text(json.dumps(documents))
    subprocess.run(
        [
            registrar,
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
    parser.add_argument("--moc", help="path to real moc, else looked up")
    parser.add_argument(
        "--qmltyperegistrar", help="path to qmltyperegistrar, else looked up"
    )
    parser.add_argument(
        "--qt-headers", default=str(QT_HEADERS), help="Qt's include prefix, for moc"
    )
    parser.add_argument(
        "--compile-dir",
        default=os.getcwd(),
        help="the directory the exporter's sources were compiled from",
    )
    args = parser.parse_args()

    moc = find_tool("moc", args.moc)
    registrar = find_tool("qmltyperegistrar", args.qmltyperegistrar)
    headers = pathlib.Path(args.qt_headers)
    for what, missing in (("moc", moc), ("qmltyperegistrar", registrar)):
        if missing is None:
            print(f"skipped: no Qt 6 {what} found")
            return 0
    if not (headers / "QtCore").is_dir():
        print(f"skipped: Qt headers not found under {headers}")
        return 0

    with tempfile.TemporaryDirectory() as raw:
        tmp = pathlib.Path(raw)

        moc_documents = drop_signal_argument_names(
            [run_moc(moc, headers, HERE / "moc-mirror.hpp", tmp / "moc_mirror.cpp")]
        )

        reflex_json = tmp / "reflex_twin.json"
        subprocess.run([args.exporter, reflex_json, "-C", args.compile_dir], check=True)
        reflex_documents = drop_signal_argument_names(
            json.loads(reflex_json.read_text())
        )

        metatypes_diff = unified(
            dump(moc_documents), dump(reflex_documents), "moc(mirror)", "reflex(twin)"
        )

        qmltypes_diff = unified(
            clean_qmltypes(
                run_registrar(registrar, moc_documents, tmp / "mirror.qmltypes", tmp)
            ),
            clean_qmltypes(
                run_registrar(registrar, reflex_documents, tmp / "twin.qmltypes", tmp)
            ),
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
