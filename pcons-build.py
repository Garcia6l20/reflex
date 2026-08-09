from pcons import Project, add_subdirectory, get_var

from reflex_build.config import VARIANT, build_dir, project_dir, toolchain

# =============================================================================
# Warnings
# =============================================================================

WARNINGS = [
    "-Wall",
    "-Wextra",
    "-Wreturn-type",
    "-Wdangling-reference",
    "-Wformat=2",
    "-Wimplicit-fallthrough",
    "-Wnon-virtual-dtor",
    "-Woverloaded-virtual",
    "-Wcast-qual",
]

if get_var("REFLEX_WERROR", True):
    WARNINGS.append("-Werror")

# =============================================================================
# Setup
# =============================================================================

project = Project("reflex", root_dir=project_dir, build_dir=build_dir)
if project.is_top_level:
    env = project.Environment(toolchain=toolchain)

    abi_version = 21

    # -fmodules is not here: the toolchain adds it to every translation unit
    # taking part in the module pass.
    env.cxx.flags.extend(
        [
            "-std=gnu++26",
            "-fimplicit-constexpr",
            "-freflection",
            f"-Wabi={abi_version}",
            f"-fabi-version={abi_version}",
            "-fmax-errors=5",
            *WARNINGS,
        ]
    )

    env.set_variant(VARIANT)
    if VARIANT == "release":
        # After the variant, so it wins over the preset's -O2.
        env.cxx.flags.append("-O3")
else:
    # A sub-project with no environment of its own inherits the enclosing
    # project's, so there is nothing to register here.
    env = project.default_environment

add_subdirectory("core")
add_subdirectory("cli")
add_subdirectory("poly")
add_subdirectory("serde")
add_subdirectory("jinja")
add_subdirectory("py")
