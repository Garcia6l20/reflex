import sys
from pathlib import Path

from pcons import context

from reflex_build.config import build_dir, project_dir
from reflex_build.qt import add_metatypes, qml_module, use_qt
from reflex_build.testing import add_test

project = context.current_project

qt_lib = project.get_target("reflex.qt")

qt_qml = use_qt(project, project.default_environment, ["Qml"])
if qt_qml is None:
    print("-- reflex.qt QML and engine tests skipped: Qt 6 Qml not found")

current_dir = project.current_dir


def qt_include_dirs(package):
    """Every include directory the package publishes, for moc's own -I list.

    use_qt moves Qt's directories to system_include_dirs, which is where moc
    would otherwise never look.
    """
    dirs = []
    for module in package.modules.values():
        dirs.extend(str(d) for d in module.public.system_include_dirs)
        dirs.extend(str(d) for d in module.public.include_dirs)
    return list(dict.fromkeys(dirs))


def moc_header(package, header):
    """Run real moc on @p header and return the generated translation unit."""
    env = project.default_environment.clone()
    env.qt.mocincludes = qt_include_dirs(package)
    out = Path(env.get("build_dir", "build")) / "qt.tests" / f"moc_{Path(header).stem}.cpp"
    return env.qt.Moc(out, str(current_dir / header))[0]


moc_qml_mirror = moc_header(qt_qml, "moc-qml-mirror.hpp") if qt_qml else None
moc_mirror = moc_header(qt_qml, "moc-mirror.hpp") if qt_qml else None


def engine_module():
    """The QML module test-engine.cpp hands to a real QQmlApplicationEngine.

    Built the way an application builds one: the metatypes document from
    engine/module.cpp, qmltyperegistrar over it, and the QML embedded next to
    the qmldir it names. Linked into the test binary, so `pcons test` reaches
    the engine without running an example.
    """
    metatypes, exporter = add_metatypes(
        "reflex-qt-engine",
        ["engine/module.cpp"],
        link=[qt_qml.Qml],
        include_roots=["."],
    )
    from_root = current_dir.relative_to(project_dir)
    types = qml_module(
        "reflex-qt-engine-types",
        project.default_environment,
        uri="Reflex.EngineTest",
        qml_files=[f"{from_root}/engine/Main.qml"],
        metatypes=metatypes,
        link=[qt_qml.Qml, qt_lib],
    )
    types.depends(exporter)
    types.private.include_dirs.append(".")
    return types


engine_types = engine_module() if qt_qml else None

for src in sorted(current_dir.glob("test-*.cpp")):
    test_name = src.stem.removeprefix("test-")
    libs = [qt_lib]
    sources = [src.name]
    if test_name == "qml":
        if qt_qml is None:
            continue
        libs.append(qt_qml.Qml)
        sources.append(moc_qml_mirror)
    if test_name == "engine":
        if engine_types is None:
            continue
        libs.extend([qt_qml.Qml, engine_types])
    if test_name == "moc-json":
        if moc_mirror is None:
            continue
        sources.append(moc_mirror)
    test = add_test(test_name, sources, libs, group="qt")
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
