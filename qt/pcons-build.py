from pcons import add_subdirectory, context
from pcons.toolchains.qt import find_qt

from reflex_build.config import build_programs, build_testing

project = context.current_project
env = project.default_environment

_patched = set()


def _patch(qt):
    for name, module in qt.modules.items():
        if name in _patched:
            continue
        _patched.add(name)
        # Qt 6.11 headers trip -Wsfinae-incomplete on GCC 16, which -Werror turns
        # fatal. Reproduces with <string> plus <QtCore/qmetatype.h> alone, so
        # -isystem is the only lever short of dropping the warning repo-wide.
        module.public.system_include_dirs.extend(module.public.include_dirs)
        module.public.include_dirs.clear()
        # This Qt is built -reduce-relocations: without -fPIC the link fails with
        # a copy relocation against the non-copyable protected symbol
        # QByteArray::_empty.
        module.public.compile_flags.append("-fPIC")


qt = find_qt(project, env, modules=["Core"], required=False)

if qt is None:
    print("-- reflex.qt skipped: Qt 6 not found")
else:
    _patch(qt)

    qt_lib = project.HeaderOnlyLibrary("reflex.qt", include_dirs=["include"])
    qt_lib.public.link_libs.extend([project.get_target("reflex.core"), qt.Core])

    if build_testing:
        add_subdirectory("tests")

    if build_programs:
        qt_widgets = find_qt(project, env, modules=["Widgets"], required=False)
        if qt_widgets is None:
            print("-- reflex.qt examples skipped: Qt 6 Widgets not found")
        else:
            _patch(qt_widgets)
            example = project.QtProgram(
                "reflex-qt-widgets",
                env,
                sources=["examples/widgets/main.cpp"],
                link=[qt_widgets.Widgets, qt_lib],
            )
            print(f"-- Example added: {example.name}")
