from pcons import context, write_file
from pcons.core.target import Target
from pcons.util.source_location import get_caller_location

from reflex_build.config import build_dir, build_testing, project_dir
from reflex_build import requirements  # noqa: F401  (registers the Conan finder)

if build_testing:
    print("Tests enabled - building test utilities")

    project = context.current_project
    env = project.default_environment

    # Get doctest package
    doctest_pkg = project.find_package("doctest", required=False)
    if not doctest_pkg:
        raise RuntimeError(
            "doctest package not found - try running:\n"
            "  conan install . --output-folder=build/conan --build=missing"
        )

    # Create a linkable library. write_file leaves the timestamp alone when the
    # content has not changed, so nothing downstream rebuilds.
    doctest_lib_impl_src = write_file(
        build_dir / "doctest/impl.cpp", "#include <doctest/doctest.h>\n"
    )

    doctest_with_main = project.StaticLibrary(
        "doctest-with-main", env, sources=[doctest_lib_impl_src]
    )
    doctest_with_main.public.link_libs.append(doctest_pkg)
    doctest_with_main.private.defines.append("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

    def binary_name(name: str, group: str | None) -> str:
        """Target names are flat across the project, so the group is part of it.

        Two directories both holding a test-basic.cpp would otherwise collide.
        """
        return f"reflex-test-{group}-{name}" if group else f"reflex-test-{name}"

    def test_name(name: str, group: str | None) -> str:
        """What the runner prints and what -R filters on."""
        return f"{group}.{name}" if group else name

    def add_test(
        name: str, sources: list[str], libs: list, group: str | None = None
    ) -> Target:
        test = project.Program(
            binary_name(name, group),
            env,
            sources=sources,
            defined_at=get_caller_location(),
        )
        test.private.include_dirs.append(".")
        test.private.link_libs.extend([*libs, doctest_with_main])
        # discover: the runner lists the binary's cases and runs each one
        # separately, so a failure names the case rather than the binary.
        project.Test(
            test_name(name, group),
            test,
            labels=[group] if group else [],
            discover="doctest",
            defined_at=get_caller_location(),
        )

        # The label filters what runs; the alias builds a group on its own.
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
        from reflex_build.python import interpreter, module_dir

        test = project.Test(
            test_name(name, group),
            interpreter,
            args=[str(script)],
            env={"PYTHONPATH": str(module_dir(project))},
            labels=[group] if group else [],
        )
        # program is an interpreter, not a Target, so the extension is not
        # pulled in by itself.
        test.add_dependency(extension)

        if group:
            project.Alias(f"test-{group}", extension)

        return test
