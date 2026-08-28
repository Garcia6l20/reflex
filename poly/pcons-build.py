from pcons import add_subdirectory, context

from reflex_build.config import build_testing
from reflex_build.library import reflex_library

project = context.current_project

reflex_library(
    "reflex.poly",
    module_sources=["modules/reflex/poly.cppm"],
    link_libs=[project.get_target("reflex.core")],
)

if build_testing:
    add_subdirectory("tests")
