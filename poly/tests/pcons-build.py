from pcons import Project, get_target
from reflex_build.testing import add_test

poly = get_target("reflex.poly")

current_dir = Project.current().current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [poly], group="poly")
    print(f"-- Test added: {test.name}")

