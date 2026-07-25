from pcons import add_subdirectory, context
from reflex_build.testing import build_testing

project = context.current_project
env = project.default_environment

jinja = project.StaticLibrary(
    "reflex.jinja",
    env,
    sources=[
        "modules/reflex/jinja.cppm",
    ],
)
jinja.public.include_dirs.append("include")
jinja.public.link_libs.extend(
    project.get_targets("reflex.core", "reflex.poly", "reflex.serde", "reflex.serde.json")
)


if build_testing:
    add_subdirectory("tests")
