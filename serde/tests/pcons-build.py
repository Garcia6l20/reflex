from pcons import Project, get_targets
from reflex_build.testing import add_test

serde, serde_json, serde_bson = get_targets("reflex.serde", "reflex.serde.json", "reflex.serde.bson")

current_dir = Project.current().current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [serde, serde_json, serde_bson], group="serde")
    print(f"-- Test added: {test.name}")

