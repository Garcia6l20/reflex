from pathlib import Path
from pcons import Project, program, static_library

from reflex_build.config import build_dir, build_testing
from reflex_build.requirements import packages

if build_testing:
    print("Tests enabled - building test utilities")

    # Get doctest package
    doctest_pkg = packages.get("doctest")
    if not doctest_pkg:
        raise RuntimeError(
            "doctest package not found - try running:\n"
            "  conan install . --output-folder=build/conan --build=missing"
        )

    # Create a linkable library
    doctest_lib_impl_src = build_dir / "doctest/impl.cpp"
    if not doctest_lib_impl_src.exists():
        doctest_lib_impl_src.parent.mkdir(parents=True, exist_ok=True)
        doctest_lib_impl_src.write_text("#include <doctest/doctest.h>\n")

    doctest_with_main = static_library(
        name="doctest-with-main", sources=[doctest_lib_impl_src]
    )
    doctest_with_main.public.link_libs.append(doctest_pkg)
    doctest_with_main.private.defines.append("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    project = Project.current()
    env = project.get_default_environment()

    import sys

    python = sys.executable.replace("\\", "/")
    runner_script = Path(__file__).parent / "_test_runner.py"

    def add_test_command(name: str):
        return env.Command(
            target=name,
            source=[runner_script],
            command=f"{python} $SOURCE",
        )

    test_command = add_test_command("test")
    group_commands = dict()

    def add_test(
        name: str, sources: list[str], libs: list, group: str | None = None
    ) -> None:
        test_prefix = "reflex-test-"
        if group:
            test_prefix += f"{group}-"

        test = program(
            f"{test_prefix}{name}",
            sources=sources,
        )
        test.private.include_dirs.append(".")
        test.private.link_libs.extend([*libs, doctest_with_main])
        project.Alias("tests", test)
        test_command.add_source(test)

        if group:
            if group not in group_commands:
                group_commands[group] = add_test_command(f"test-{group}")

            group_commands[group].add_source(test)

        return test
