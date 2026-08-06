from pcons import add_subdirectory, context

from reflex_build.python import nanobind_library
from reflex_build.testing import build_testing

project = context.current_project
env = project.default_environment

nanobind = nanobind_library(project, env)

py = project.StaticLibrary(
    "reflex.py",
    env,
    sources=[
        "modules/reflex/py.cppm",
    ],
)
py.public.include_dirs.append("include")
py.public.link_libs.extend([project.get_target("reflex.core"), nanobind])


if build_testing:
    add_subdirectory("tests")
