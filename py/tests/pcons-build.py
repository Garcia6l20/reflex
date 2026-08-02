from pcons import context
from reflex_build.python import add_python_extension
from reflex_build.testing import add_python_test

project = context.current_project
env = project.default_environment

nanobind = project.get_target("nanobind.runtime")

hello = add_python_extension(project, env, "hello", ["ext/hello.cpp"], [nanobind])

add_python_test(
    "hello",
    hello,
    project.current_dir / "python" / "test_hello.py",
    group="py",
)
