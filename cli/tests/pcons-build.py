from pcons import program, get_target

fake_git = program(
    "reflex-fake-git",
    sources=["fake-git.cpp"],
)
fake_git.private.include_dirs.append("include")
fake_git.private.link_libs.append(get_target("reflex.cli"))
