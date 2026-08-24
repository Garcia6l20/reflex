import platform
from pathlib import Path
from xml.sax.saxutils import escape

import pcons
from pcons import PathToken, context
from pcons.toolchains.qt import find_qt

from reflex_build.config import build_dir

_patched = set()

PCONS_TESTED = (0, 28)
_scan_warned = False


def use_qt(project, env, modules, *, required=False):
    """Find Qt and apply the two workarounds this toolchain needs.

    Qt 6.11 headers trip ``-Wsfinae-incomplete`` on GCC 16, which ``-Werror``
    turns fatal. It reproduces with ``<string>`` plus ``<QtCore/qmetatype.h>``
    alone, so moving Qt's include directories to ``system_include_dirs`` is the
    only lever short of dropping the warning repository-wide.

    This Qt is built ``-reduce-relocations``: without ``-fPIC`` the link fails
    with a copy relocation against the non-copyable protected symbol
    ``QByteArray::_empty``.

    Returns what ``find_qt`` returns, ``None`` included.
    """
    qt = find_qt(project, env, modules=modules, required=required)
    if qt is None:
        return None
    for name, module in qt.modules.items():
        if name in _patched:
            continue
        _patched.add(name)
        module.public.system_include_dirs.extend(module.public.include_dirs)
        module.public.include_dirs.clear()
        module.public.compile_flags.append("-fPIC")
    return qt


def add_metatypes(name, sources, *, env=None, link=(), include_roots=()):
    """Declare the metatypes exporter for a REFLEX_QT_MODULE translation unit.

    ``sources`` is the TU holding the module body, whose ``main`` calls
    ``reflex::qt::moc::write_metatypes<module>(argv[1], opts)``. It is built as
    a program, run once, and its JSON document is what
    ``QtQmlModule(metatypes=[...])`` consumes.

    ``include_roots`` are the exporter's include directories, and the ones it
    spells ``inputFile`` relative to. Pass the same ones the QML module
    compiles the generated registration with: without them
    the generated registration carries the absolute header path of the machine
    that built it. A relative root is taken from the build script's directory,
    as pcons takes ``include_dirs``.

    ``-C`` is the directory the compiler ran in. ninja compiles with relative
    paths, so that is what ``std::source_location`` records and what the
    exporter needs to complete them; it refuses to guess it from its own
    working directory.

    Returns the document's path and the command target that writes it.
    """
    project = context.current_project
    env = env or project.default_environment

    exporter = project.Program(f"{name}-export", env, sources=list(sources))
    exporter.private.link_libs.append(project.get_target("reflex.qt"))
    exporter.private.link_libs.extend(link)
    exporter.private.include_dirs.extend(include_roots)

    output = project.build_dir / "qt.metatypes" / f"{name}.json"
    roots = [Path(r) for r in include_roots]
    flags = [
        token
        for root in roots
        for token in (
            "-I",
            str(root if root.is_absolute() else project.current_dir / root),
        )
    ]

    run = "" if platform.system() == "Windows" else "./"
    command = env.Command(
        target=output,
        source=[exporter],
        command=[f"{run}${{SOURCES[0]}}", "$TARGET", "-C", str(build_dir), *flags],
        name=f"{name}-metatypes",
        write_if_different=True,
    )
    return output, command


def _write_if_changed(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != content:
        path.write_text(content, encoding="utf-8")


def _qrc_document(prefix, entries):
    lines = ["<RCC>", f'    <qresource prefix="{escape(prefix)}">']
    for alias, path in entries:
        lines.append(
            f'        <file alias="{escape(alias)}">{escape(str(path))}</file>'
        )
    lines += ["    </qresource>", "</RCC>", ""]
    return "\n".join(lines)


def _set_node_vars(node, node_vars):
    info = getattr(node, "_build_info", None)
    if info is not None:
        info["vars"] = node_vars


def _pcons_version():
    """The major and minor of the running pcons, or PCONS_TESTED when unreadable.

    A pre-release or local segment is not an integer, and a version this cannot
    read is not a reason to fail a configure.
    """
    parts = pcons.__version__.split(".")[:2]
    try:
        return tuple(int(part) for part in parts)
    except ValueError:
        return PCONS_TESTED


def generated_env(env):
    """A clone of @p env that takes no part in the C++ module scan.

    pcons scans every C++ source of an environment that has a module
    interface in it, and the scan is one build edge feeding one dyndep file.
    A generated source in that set makes the dyndep depend on whatever
    produces it, and the qmltyperegistrar output is produced by running a
    program built from scanned sources, which closes a cycle ninja refuses to
    build. The clone qualifies for nothing, so its objects compile outside the
    scan, since it holds no module interface of its own. Nothing generated
    here imports a module.
    """
    global _scan_warned

    if not _scan_warned and _pcons_version() > PCONS_TESTED:
        _scan_warned = True
        tested = ".".join(str(part) for part in PCONS_TESTED)
        print(
            f"-- reflex.qt: the QML module clones its environment to dodge a dyndep"
            f" cycle in pcons {tested}; pcons {pcons.__version__} is newer, check"
            " whether the clone is still needed"
        )
    return env.clone()


def qml_module(name, env, *, uri, qml_files, metatypes, link=(), version="1.0"):
    """Build a QML module whose types come from a metatypes document.

    The reflex equivalent of ``project.QtQmlModule``, for a module moc never
    sees: ``metatypes`` is the document ``add_metatypes`` writes, and it goes
    to ``qmltyperegistrar`` as it is. A ``Q_OBJECT`` class in the same module
    is therefore not published - merging the two documents is what stock
    ``QtQmlModule`` would have to do, and no caller needs it.

    ``qml_files`` are project-root relative, as stock ``QtQmlModule`` takes
    them, and alone among the paths a build script writes: ``sources``,
    ``include_dirs`` and ``add_metatypes``'s ``include_roots`` are all relative
    to the script's own directory. A caller naming one directory twice should
    derive the root-relative spelling from ``project.current_dir`` rather than
    write it out. Each file is embedded under ``:/qt/qml/<uri as path>/`` next
    to a synthesized ``qmldir`` and the generated ``.qmltypes``, and its stem
    names the QML type.

    Returns an object target, so ``app.link(module)`` pulls the registration
    and the resources into the application.
    """
    project = context.current_project
    if not env.has_tool("qt"):
        raise RuntimeError(
            "qml_module() needs the qt toolchain on the environment. "
            "Call use_qt(project, env, [...]) first."
        )

    major, _, minor = version.partition(".")
    minor = minor or "0"
    uri_path = uri.replace(".", "/")
    root = project.root_dir
    qt_dir = Path(env.get("build_dir", "build")) / f"qt.{name}"
    qmltypes_name = f"{name}.qmltypes"

    target = project.ObjectLibrary(name, generated_env(env), sources=[])
    if link:
        target.link(*link)

    registrar = env.qt.TypeRegistrar(
        qt_dir / f"{name}_qmltyperegistrations.cpp", [metatypes]
    )[0]
    _set_node_vars(
        registrar,
        {
            "QMLURI": uri,
            "QMLMAJOR": major,
            "QMLMINOR": minor,
            "QMLTYPES": PathToken(path=f"qt.{name}/{qmltypes_name}", path_type="build"),
            "QMLFOREIGN": [],
        },
    )

    qmldir = [
        f"module {uri}",
        f"typeinfo {qmltypes_name}",
        f"prefer :/qt/qml/{uri_path}/",
    ]
    qmldir += [
        f"{Path(qml).stem} {major}.{minor} {Path(qml).name}" for qml in qml_files
    ]
    _write_if_changed(root / qt_dir / "qmldir", "\n".join(qmldir) + "\n")

    entries = [(Path(qml).name, root / qml) for qml in qml_files]
    entries.append(("qmldir", root / qt_dir / "qmldir"))
    entries.append((qmltypes_name, root / qt_dir / qmltypes_name))
    qrc = qt_dir / f"{name}.qrc"
    _write_if_changed(root / qrc, _qrc_document(f"/qt/qml/{uri_path}", entries))

    resources = env.qt.Rcc(qt_dir / f"qrc_{name}.cpp", qrc, name=f"qml_{name}")[0]
    resources.implicit_deps.append(registrar)

    target.add_sources([registrar, resources])
    return target
