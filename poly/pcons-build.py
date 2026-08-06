from pcons import add_subdirectory, context

from reflex_build.testing import build_testing

project = context.current_project
env = project.default_environment

poly = project.StaticLibrary(
    "reflex.poly",
    env,
    sources=[
        "modules/reflex/poly.cppm",
    ],
)
poly.public.include_dirs.append("include")
poly.public.link_libs.append(project.get_target("reflex.core"))


if build_testing:
    add_subdirectory("tests")
