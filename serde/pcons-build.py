from pcons import add_subdirectory, static_library, get_targets
from reflex_build.testing import build_testing

serde = static_library(
    "reflex.serde",
    sources=[
        "modules/reflex/serde.cppm",
    ],
)
serde.public.include_dirs.append("include")
serde.public.link_libs.extend(get_targets("reflex.poly", "reflex.core"))

serde_json = static_library(
    "reflex.serde.json",
    sources=[
        "modules/reflex/serde_json.cppm",
    ],
)
serde_json.public.link_libs.append(serde)

serde_bson = static_library(
    "reflex.serde.bson",
    sources=[
        "modules/reflex/serde_bson.cppm",
    ],
)
serde_bson.public.link_libs.append(serde)

if build_testing:
    add_subdirectory("tests")
