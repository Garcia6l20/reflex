import sys
import sysconfig
from pathlib import Path

from pcons import ImportedTarget
from pcons.packages.description import PackageDescription

from reflex_build.config import build_dir

# =============================================================================
# Fetched packages
# =============================================================================

pkg_dir = Path(build_dir) / "pkg"

FETCH_HINT = (
    "run:\n"
    "  python3 -m pcons.packages.fetch.cli fetch deps.toml"
    f" --deps-dir {build_dir}/deps --output-dir {pkg_dir}"
)


def _fetched(name: str) -> tuple[ImportedTarget, Path]:
    """The imported target for a fetched package, and its unpacked source tree."""
    description = pkg_dir / f"{name}.pcons-pkg.toml"
    if not description.exists():
        raise RuntimeError(f"{name} package not found - {FETCH_HINT}")
    package = PackageDescription.from_toml(description)
    return ImportedTarget.from_package(package), Path(package.prefix)


# =============================================================================
# Interpreter
# =============================================================================

# Everything here describes one interpreter, the one pcons is running under.
# Building against its headers and importing with another is the failure mode
# this pins down.
interpreter = sys.executable

python_include = sysconfig.get_paths()["include"]

# ".cpython-314-x86_64-linux-gnu.so" - an extension not carrying it is invisible
# to the import machinery.
ext_suffix = sysconfig.get_config_var("EXT_SUFFIX")


# =============================================================================
# Targets
# =============================================================================


def nanobind_library(project, env):
    """The amalgamated nanobind runtime, built once and shared by every extension.

    NB_DOMAIN is public: the type registry is keyed on it, so a lookup crossing
    a translation unit that saw a different value silently misses.
    """
    nanobind, prefix = _fetched("nanobind")
    robin_map, _ = _fetched("robin-map")

    # Not "nanobind": ImportedTarget already claims the package's own name.
    lib = project.StaticLibrary(
        "nanobind.runtime", env, sources=[prefix / "src" / "nb_combined.cpp"]
    )
    lib.public.link_libs.extend([nanobind, robin_map])
    # -isystem, not an include_dir and not -I: the module scan command rewrites
    # every -I as project relative, which turns an out-of-tree path into
    # "..//usr/include/python3.14". -isystem is left alone, and it silences the
    # CPython headers' own warnings as well.
    lib.public.compile_flags.append(f"-isystem{python_include}")
    lib.public.defines.append('NB_DOMAIN="reflex"')
    # -fPIC because the archive is linked into a shared object. Without it the
    # linker reports "failed to set dynamic section sizes: bad value", which
    # names neither the flag nor the file.
    lib.private.compile_flags.extend(
        ["-fPIC", "-fno-strict-aliasing", "-fvisibility=hidden"]
    )
    return lib


def add_python_extension(project, env, name: str, sources: list, libs: list):
    """A CPython extension module, importable as `name`.

    Nothing links libpython: on Linux and macOS the interpreter supplies those
    symbols, and linking it explicitly is how a second copy of the interpreter
    state gets in.
    """
    ext = project.SharedLibrary(name, env, sources=sources)
    ext.output_prefix = ""
    ext.output_suffix = ext_suffix
    ext.private.compile_flags.append("-fvisibility=hidden")
    ext.private.link_libs.extend(libs)
    return ext
