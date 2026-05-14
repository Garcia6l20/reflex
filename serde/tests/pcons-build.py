from pcons import context
from reflex_build.testing import add_test

project = context.current_project

serde, serde_json, serde_bson = project.get_targets(
    "reflex.serde", "reflex.serde.json", "reflex.serde.bson"
)

current_dir = project.current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(
        test_name, [src.name], [serde, serde_json, serde_bson], group="serde"
    )
    print(f"-- Test added: {test.name}")
