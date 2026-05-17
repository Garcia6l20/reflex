from pcons import add_subdirectory, static_library, get_targets
from reflex_build.testing import build_testing

jinja = static_library(
    "reflex.jinja",
    sources=[
        "modules/reflex/jinja.cppm",
    ],
)
jinja.public.include_dirs.append("include")
jinja.public.link_libs.extend(get_targets("reflex.core", "reflex.poly"))


if build_testing:
    add_subdirectory("tests")
