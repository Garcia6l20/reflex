from pcons import add_subdirectory, context

from reflex_build.config import build_testing
from reflex_build.library import reflex_library
from reflex_build.python import nanobind_library

project = context.current_project
env = project.default_environment

nanobind = nanobind_library(project, env)

reflex_library(
    "reflex.py",
    module_sources=["modules/reflex/py.cppm"],
    link_libs=[project.get_target("reflex.core"), nanobind],
)

if build_testing:
    add_subdirectory("tests")
