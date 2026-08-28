from pcons import add_subdirectory, context

from reflex_build.config import build_testing
from reflex_build.library import reflex_library

project = context.current_project

reflex_library(
    "reflex.jinja",
    module_sources=["modules/reflex/jinja.cppm"],
    link_libs=project.get_targets(
        "reflex.core", "reflex.poly", "reflex.serde", "reflex.serde.json"
    ),
)

if build_testing:
    add_subdirectory("tests")
