from pcons import context

from reflex_build.testing import add_test

project = context.current_project

qt_lib = project.get_target("reflex.qt")

current_dir = project.current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [qt_lib], group="qt")
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
