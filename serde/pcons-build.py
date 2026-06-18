from pcons import add_subdirectory, context
from reflex_build.testing import build_testing

project = context.current_project
env = project.default_environment

serde = project.StaticLibrary(
    "reflex.serde",
    env,
    sources=[
        "modules/reflex/serde.cppm",
    ],
)
serde.public.include_dirs.append("include")
serde.public.link_libs.extend(project.get_targets("reflex.poly", "reflex.core"))

serde_json = project.StaticLibrary(
    "reflex.serde.json",
    env,
    sources=[
        "modules/reflex/serde_json.cppm",
    ],
)
serde_json.public.link_libs.append(serde)

serde_bson = project.StaticLibrary(
    "reflex.serde.bson",
    env,
    sources=[
        "modules/reflex/serde_bson.cppm",
    ],
)
serde_bson.public.link_libs.append(serde)

serde_csv = project.StaticLibrary(
    "reflex.serde.csv",
    env,
    sources=[
        "modules/reflex/serde_csv.cppm",
    ],
)
serde_csv.public.link_libs.append(serde)

if build_testing:
    add_subdirectory("tests")
