from pcons import add_subdirectory, context

from reflex_build.config import build_programs, build_testing
from reflex_build.library import reflex_library

project = context.current_project

serde = reflex_library(
    "reflex.serde",
    module_sources=["modules/reflex/serde.cppm"],
    link_libs=project.get_targets("reflex.poly", "reflex.core"),
)

for backend in ("json", "bson", "csv", "xml", "yaml", "toml"):
    reflex_library(
        f"reflex.serde.{backend}",
        module_sources=[f"modules/reflex/serde_{backend}.cppm"],
        include_dir=None,
        link_libs=[serde],
    )

if build_programs:
    add_subdirectory("programs")

if build_testing:
    add_subdirectory("tests")
