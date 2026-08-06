from pcons import context

from reflex_build.testing import add_test

project = context.current_project
env = project.default_environment

jinja = project.get_target("reflex.jinja")

types = project.StaticLibrary(
    "jinja-test-types",
    env,
    sources=[
        "types.cppm",
        "types.cpp",
    ],
)
types.public.include_dirs.append("include")
types.public.link_libs.append(jinja)


current_dir = project.current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [jinja, types], group="jinja")
    print(f"-- Test added: {test.name}")

