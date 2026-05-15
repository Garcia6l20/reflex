from pcons import Project, program, get_target, static_library
from reflex_build.testing import add_test

cli = get_target("reflex.cli")

fake_git = program(
    "reflex-fake-git",
    sources=["fake-git.cpp"],
)
fake_git.private.include_dirs.append("include")
fake_git.private.link_libs.append(cli)

testutils = static_library(name="reflex-testutils", sources=["src/testutils.cpp"])
testutils.public.include_dirs.append("include")
testutils.public.link_libs.append(cli)

current_dir = Project.current().current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [cli, testutils], group="cli")
    print(f"-- Test added: {test.name}")

