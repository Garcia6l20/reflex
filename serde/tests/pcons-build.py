from pcons import context

from reflex_build.testing import add_test

project = context.current_project

serde, serde_json, serde_bson, serde_csv, serde_xml, serde_yaml = project.get_targets(
    "reflex.serde",
    "reflex.serde.json",
    "reflex.serde.bson",
    "reflex.serde.csv",
    "reflex.serde.xml",
    "reflex.serde.yaml",
)

env = project.default_environment

types = project.StaticLibrary(
    "serde-test-types",
    env,
    sources=[
        "types.cppm",
    ],
)
types.public.link_libs.append(serde)

current_dir = project.current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(
        test_name,
        [src.name],
        [serde, serde_json, serde_bson, serde_csv, serde_xml, serde_yaml, types],
        group="serde",
    )
    print(f"-- Test added: {test.name}")
