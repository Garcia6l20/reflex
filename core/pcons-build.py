from pcons import add_subdirectory, static_library
from reflex_build.testing import build_testing

static_library(
    "reflex.core",
    sources=[
        "modules/reflex/core.cppm",
    ],
).public.include_dirs.append("include")

if build_testing:
    add_subdirectory("tests")
