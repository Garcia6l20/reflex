from pcons import Project, get_target, static_library
from reflex_build.testing import add_test

jinja = get_target("reflex.jinja")

types = static_library(
    "jinja-test-types",
    sources=[
        "types.cppm",
        "types.cpp",
    ],
)
types.public.include_dirs.append("include")
types.public.link_libs.append(jinja)


current_dir = Project.current().current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [jinja, types], group="jinja")
    print(f"-- Test added: {test.name}")

