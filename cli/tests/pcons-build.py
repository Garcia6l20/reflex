from pcons import program, get_target, static_library
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

add_test("git", ["test-git.cpp"], [cli, testutils])
