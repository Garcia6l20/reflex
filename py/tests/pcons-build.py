from pcons import context
from reflex_build.python import add_python_extension
from reflex_build.testing import add_python_test, add_test

project = context.current_project
env = project.default_environment

nanobind = project.get_target("nanobind.runtime")
py = project.get_target("reflex.py")

for src in project.current_dir.glob("test-*.cpp"):
    test = add_test(src.stem.removeprefix("test-"), [src.name], [py], group="py")
    print(f"-- Test added: {test.name}")

hello = add_python_extension(project, env, "hello", ["ext/hello.cpp"], [nanobind])

add_python_test(
    "hello",
    hello,
    project.current_dir / "python" / "test_hello.py",
    group="py",
)
