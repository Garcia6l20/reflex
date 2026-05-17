from pcons import add_subdirectory, static_library, get_target
from reflex_build.testing import build_testing

cli = static_library(
    "reflex.poly",
    sources=[
        "modules/reflex/poly.cppm",
    ],
)
cli.public.include_dirs.append("include")
cli.public.link_libs.append(get_target("reflex.core"))


if build_testing:
    add_subdirectory("tests")
