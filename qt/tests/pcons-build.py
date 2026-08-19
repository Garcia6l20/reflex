from pcons import context

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

# Not a doctest: moc-cross-check.py runs it against real moc and qmltyperegistrar,
# which the suite must not depend on being installed.
exporter = project.Program(
    "reflex-qt-moc-export",
    project.default_environment,
    sources=["moc-export.cpp"],
)
exporter.private.include_dirs.append(".")
exporter.private.link_libs.append(qt_lib)
print(f"-- Program added: {exporter.name}")
