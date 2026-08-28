import os
from pathlib import Path

from pcons import Configure, find_c_toolchain, get_var, get_variant

# =============================================================================
# Configuration
# =============================================================================

VARIANT = get_variant("release")

project_dir = Path(__file__).parent.parent
build_dir = Path(os.environ.get("PCONS_BUILD_DIR", project_dir / "build"))

config = Configure(build_dir=build_dir)
toolchain = find_c_toolchain(prefer=["gcc"])

coverage = get_var("REFLEX_COVERAGE", False)
build_modules = get_var("REFLEX_MODULES", True)

_want_testing = get_var("REFLEX_BUILD_TESTS", False) or coverage
_want_programs = get_var("REFLEX_BUILD_PROGRAMS", False)

if coverage and not build_modules:
    raise RuntimeError(
        "REFLEX_COVERAGE needs tests, and the test sources import modules: "
        "it cannot be combined with REFLEX_MODULES=false"
    )

if not build_modules and (_want_testing or _want_programs):
    print(
        "REFLEX_MODULES=false: tests and programs disabled, their sources import modules"
    )

build_testing = _want_testing and build_modules
build_programs = _want_programs and build_modules

# =============================================================================
# Libraries
# =============================================================================

LIBRARY_DEPENDENCIES: dict[str, tuple[str, ...]] = {
    "cli": (),
    "poly": (),
    "serde": ("poly",),
    "jinja": ("serde",),
    "py": (),
}


def _option(name: str) -> str:
    return f"REFLEX_{name.upper()}"


def _resolve_libraries() -> dict[str, bool]:
    """Which optional libraries are built, after propagating what each needs.

    Every option defaults to on, so the only interesting case is one turned off:
    disabling a library disables whatever depends on it, transitively, with a
    printed notice. Asking for a library and disabling something it needs in the
    same run is a contradiction rather than a preference, so it raises.

    `reflex.core` is not an option. Everything needs it.
    """
    requested = {
        name: get_var(_option(name), type=bool) for name in LIBRARY_DEPENDENCIES
    }
    enabled = {name: True if want is None else want for name, want in requested.items()}

    while True:
        for name, deps in LIBRARY_DEPENDENCIES.items():
            missing = [dep for dep in deps if not enabled[dep]]
            if not enabled[name] or not missing:
                continue
            named = ", ".join(_option(dep) for dep in missing)
            if requested[name]:
                raise RuntimeError(
                    f"{_option(name)} needs {named}, which the same run disables"
                )
            enabled[name] = False
            print(f"{_option(name)} disabled: it needs {named}")
            break
        else:
            return enabled


libraries = _resolve_libraries()
