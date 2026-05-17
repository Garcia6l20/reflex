from pcons import (
    add_subdirectory,
    Project,
    Environment,
)

from reflex_build.config import project_dir, build_dir, toolchain, VARIANT

# =============================================================================
# Setup
# =============================================================================

project = Project("reflex", root_dir=project_dir, build_dir=build_dir)
env = Environment(toolchain=toolchain)

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

if VARIANT == "debug":
    env.cxx.flags.extend(["-g", "-O0"])
elif VARIANT == "release":
    env.cxx.flags.append("-O3")

add_subdirectory("core")
add_subdirectory("cli")
add_subdirectory("poly")
add_subdirectory("serde")
add_subdirectory("jinja")
