from pcons import add_subdirectory, Project, Environment, find_c_toolchain

Project("reflex")
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

add_subdirectory("core")
add_subdirectory("cli")
