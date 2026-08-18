from pcons import add_subdirectory, context
from pcons.toolchains.qt import find_qt

from reflex_build.config import build_testing

project = context.current_project
env = project.default_environment

qt = find_qt(project, env, modules=["Core"], required=False)

if qt is None:
    print("-- reflex.qt skipped: Qt 6 not found")
else:
    qt_lib = project.HeaderOnlyLibrary("reflex.qt", include_dirs=["include"])
    qt_lib.public.link_libs.extend([project.get_target("reflex.core"), qt.Core])

    if build_testing:
        add_subdirectory("tests")
