from pcons import program, static_library

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


    def add_test(name: str, sources: list[str], libs: list) -> None:
        test = program(
            f"reflex-test-{name}",
            sources=sources,
        )
        test.private.include_dirs.append(".")
        test.private.link_libs.extend([*libs, doctest_with_main])
        return test
