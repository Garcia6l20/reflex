from pcons import add_subdirectory, static_library, get_target, get_var

cli = static_library(
    "reflex.cli",
    sources=[
        "modules/reflex/cli.cppm",
        "src/cli.cpp",
    ],
)
cli.public.include_dirs.append("include")
cli.public.link_libs.append(get_target("reflex.core"))

if get_var("REFLEX_BUILD_TESTS"):
    add_subdirectory("tests")
