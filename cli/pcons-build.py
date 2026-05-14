from pcons import add_subdirectory, context
from reflex_build.testing import build_testing

project = context.current_project
env = project.default_environment

cli = project.StaticLibrary(
    "reflex.cli",
    env,
    sources=[
        "modules/reflex/cli.cppm",
        "src/cli.cpp",
    ],
)
cli.public.include_dirs.append("include")
cli.public.link_libs.append(project.get_target("reflex.core"))


if build_testing:
    add_subdirectory("tests")
