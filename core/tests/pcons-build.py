from pcons import Project, context

from reflex_build.testing import add_test

core = context.get_target("reflex.core")

# Tests holding nothing but static_asserts: compiling them is the test, and they
# register no doctest case for the runner to find.
compile_only = {"derive", "meta-member-named", "meta-annotation-templates"}

current_dir = Project.current().current_dir
for src in current_dir.glob("test-*.cpp"):
    test_name = src.stem.removeprefix("test-")
    test = add_test(
        test_name,
        [src.name],
        [core],
        group="core",
        discover=None if test_name in compile_only else "doctest",
    )
    print(f"-- Test added: {test.name}")

