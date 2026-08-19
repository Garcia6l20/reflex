import platform
from pathlib import Path

from pcons import context
from pcons.toolchains.qt import find_qt

_patched = set()


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
        for token in ("-I", str(root if root.is_absolute() else project.current_dir / root))
    ]

    run = "" if platform.system() == "Windows" else "./"
    command = env.Command(
        target=output,
        source=[exporter],
        command=[f"{run}${{SOURCES[0]}}", "$TARGET", *flags],
        name=f"{name}-metatypes",
        write_if_different=True,
    )
    return output, command
