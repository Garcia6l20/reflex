from pcons import context

from reflex_build.testing import add_test

project = context.current_project
env = project.default_environment

cli = project.get_target("reflex.cli")

fake_git = project.Program(
    "reflex-fake-git",
    env,
    sources=["fake-git.cpp"],
)
fake_git.private.include_dirs.append("include")
fake_git.private.link_libs.append(cli)

testutils = project.StaticLibrary(
    "reflex-testutils", env, sources=["src/testutils.cpp"]
)
testutils.public.include_dirs.append("include")
testutils.public.link_libs.append(cli)

for src in project.current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(test_name, [src.name], [cli, testutils], group="cli")
    print(f"-- Test added: {test.name}")
