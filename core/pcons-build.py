from pcons import add_subdirectory

from reflex_build.config import build_testing
from reflex_build.library import reflex_library

reflex_library("reflex.core", module_sources=["modules/reflex/core.cppm"])

if build_testing:
    add_subdirectory("tests")
