import os
from pathlib import Path

from pcons import (
    Configure,
    ImportedTarget,
    add_subdirectory,
    Project,
    Environment,
    find_c_toolchain,
    get_variant,
    static_library,
)
from pcons.packages.finders import ConanFinder

# =============================================================================
# Configuration
# =============================================================================

VARIANT = get_variant("release")

project_dir = Path(os.environ.get("PCONS_SOURCE_DIR", Path(__file__).parent))
build_dir = Path(os.environ.get("PCONS_BUILD_DIR", project_dir / "build"))

# =============================================================================
# Setup
# =============================================================================

config = Configure(build_dir=build_dir)
toolchain = find_c_toolchain()

if not config.get("configured") or os.environ.get("PCONS_RECONFIGURE"):
    toolchain.configure(config)
    config.set("configured", True)
    config.save()

project = Project("reflex", root_dir=project_dir, build_dir=build_dir)
env = Environment(toolchain=find_c_toolchain(prefer=["gcc"]))

abi_version = 21

env.cxx.flags.extend(
    [
        "-std=gnu++26",
        "-fmodules",
        "-fimplicit-constexpr",
        "-freflection",
        f"-Wabi={abi_version}",
        f"-fabi-version={abi_version}",
        "-fmax-errors=5",
    ]
)

# =============================================================================
# Find Conan packages
# =============================================================================

# Create finder - compiler version is auto-detected
conan = ConanFinder(
    config,
    conanfile=project_dir / "conanfile.txt",
    output_folder=build_dir / "conan",
)

# Sync profile with toolchain - this generates the Conan profile file
conan.sync_profile(toolchain, build_type=VARIANT.capitalize())

# Install packages - cmake_layout subfolders are auto-searched
packages = conan.install()

# Get doctest package
doctest_pkg = ImportedTarget.from_package(packages.get("doctest"))
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

doctest_with_main = static_library(name="doctest-with-main", sources=[doctest_lib_impl_src])
doctest_with_main.public.link_libs.append(doctest_pkg)
doctest_with_main.private.defines.append("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

add_subdirectory("core")
add_subdirectory("cli")
