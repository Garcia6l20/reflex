from pcons import add_subdirectory, context

from reflex_build.testing import build_testing

project = context.current_project
env = project.default_environment
project.StaticLibrary(
    "reflex.core",
    env,
    sources=[
        "modules/reflex/core.cppm",
    ],
).public.include_dirs.append("include")

if build_testing:
    add_subdirectory("tests")
