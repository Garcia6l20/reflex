from pcons import context
from pcons.core.target import Target
from pcons.util.source_location import get_caller_location

from reflex_build.config import build_dir, build_testing, project_dir
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

    project = context.current_project
    env = project.default_environment

    doctest_with_main = project.StaticLibrary(
        "doctest-with-main", env, sources=[doctest_lib_impl_src]
    )
    doctest_with_main.public.link_libs.append(doctest_pkg)
    doctest_with_main.private.defines.append("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    group_commands = dict()

    def add_test(
        name: str, sources: list[str], libs: list, group: str | None = None
    ) -> Target:
        test_prefix = "reflex-test-"
        if group:
            test_prefix += f"{group}-"

        test = project.Program(
            f"{test_prefix}{name}",
            env,
            sources=sources,
            defined_at=get_caller_location(),
        )
        test.private.include_dirs.append(".")
        test.private.link_libs.extend([*libs, doctest_with_main])
        project.Test(f"{test_prefix}{name}", test, defined_at=get_caller_location())

        if group:
            project.Alias(f"test-{group}", test)

        return test

    def add_python_test(
        name: str, extension: Target, script, group: str | None = None
    ) -> Target:
        """Run a Python script against a built extension module.

        @p script is an absolute path: a Test's args are opaque strings and get
        no source-directory resolution.

        The interpreter is the one pcons itself runs under, which is also the
        one reflex_build.python read EXT_SUFFIX and the headers from. Any other
        interpreter would fail to import what was just built.
        """
        from reflex_build.python import interpreter

        test_prefix = "reflex-test-"
        if group:
            test_prefix += f"{group}-"

        # A target lands under the build directory at its source directory's
        # relative path. output_nodes would say so exactly, but the resolver
        # fills those in after this script has run.
        module_dir = build_dir / project.current_dir.relative_to(project_dir)

        test = project.Test(
            f"{test_prefix}{name}",
            interpreter,
            args=[str(script)],
            env={"PYTHONPATH": str(module_dir)},
        )
        # program is an interpreter, not a Target, so the extension is not
        # pulled in by itself.
        test.add_dependency(extension)

        if group:
            project.Alias(f"test-{group}", extension)

        return test
