from pcons import Project, program, get_targets, static_library

cli, doctest_with_main = get_targets("reflex.cli", "doctest-with-main")

fake_git = program(
    "reflex-fake-git",
    sources=["fake-git.cpp"],
)
fake_git.private.include_dirs.append("include")
fake_git.private.link_libs.append(cli)

env = Project.current().get_default_environment()

testutils = static_library(name="reflex-testutils", sources=["src/testutils.cpp"])
testutils.public.include_dirs.append("include")
testutils.public.link_libs.append(cli)

def add_test(name, sources):
    test = program(
        f"reflex-test-{name}",
        sources=sources,
    )
    test.private.include_dirs.append("include")
    test.private.link_libs.extend([cli, testutils, doctest_with_main])

add_test("git", ["test-git.cpp"])
