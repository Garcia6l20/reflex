from pcons import context
from pcons.packages.finders import ConanFinder

from reflex_build.config import project_dir, build_dir, config, toolchain, VARIANT

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
conan.install()

# Ahead of the pkg-config and system finders pcons chains by default, so
# project.find_package() answers from Conan first.
context.current_project.add_package_finder(conan)
