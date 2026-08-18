from pcons import context

from reflex_build.testing import add_test

project = context.current_project

qt_lib = project.get_target("reflex.qt")

current_dir = project.current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [qt_lib], group="qt")
    print(f"-- Test added: {test.name}")
