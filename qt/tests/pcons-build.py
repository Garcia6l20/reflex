import sys
from pathlib import Path

from pcons import context

from reflex_build.config import build_dir, project_dir
from reflex_build.qt import use_qt
from reflex_build.testing import add_test

project = context.current_project

qt_lib = project.get_target("reflex.qt")

qt_qml = use_qt(project, project.default_environment, ["Qml"])
if qt_qml is None:
    print("-- reflex.qt QML test skipped: Qt 6 Qml not found")

current_dir = project.current_dir
for src in sorted(current_dir.glob("test-*.cpp")):
    test_name = src.stem.removeprefix("test-")
    libs = [qt_lib]
    if test_name == "qml":
        if qt_qml is None:
            continue
        libs.append(qt_qml.Qml)
    test = add_test(test_name, [src.name], libs, group="qt")
    print(f"-- Test added: {test.name}")

exporter = project.Program(
    "reflex-qt-moc-export",
    project.default_environment,
    sources=["moc-export.cpp"],
)
exporter.private.include_dirs.append(".")
exporter.private.link_libs.append(qt_lib)
print(f"-- Program added: {exporter.name}")


def qt_header_root(package):
    """The include directory holding QtCore, which is what moc needs on -I."""
    for module in package.modules.values():
        for directory in module.public.system_include_dirs:
            if (Path(directory) / "QtCore").is_dir():
                return Path(directory)
    return None


qt_pkg = qt_qml or use_qt(project, project.default_environment, ["Core"])
moc = qt_pkg.tool_path("moc") if qt_pkg else None
registrar = qt_pkg.tool_path("qmltyperegistrar") if qt_pkg else None
headers = qt_header_root(qt_pkg) if qt_pkg else None

if moc and registrar and headers:
    cross_check = project.Test(
        "qt.moc-cross-check",
        sys.executable,
        args=[
            str(current_dir / "moc-cross-check.py"),
            str(build_dir / current_dir.relative_to(project_dir) / exporter.name),
            "--moc",
            str(moc),
            "--qmltyperegistrar",
            str(registrar),
            "--qt-headers",
            str(headers),
        ],
        labels=["qt"],
    )
    cross_check.add_dependency(exporter)
    project.Alias("test-qt", exporter)
    print(f"-- Test added: {cross_check.name}")
else:
    print(
        "-- reflex.qt moc cross-check skipped: Qt 6 moc or qmltyperegistrar not found"
    )
