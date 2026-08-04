import sys
import sysconfig
from pathlib import Path

from pcons import ImportedTarget
from pcons.packages.description import PackageDescription

from reflex_build.config import build_dir, project_dir

# =============================================================================
# Fetched packages
# =============================================================================

pkg_dir = Path(build_dir) / "pkg"

FETCH_HINT = (
    "run:\n"
    "  python3 -m pcons.packages.fetch.cli fetch deps.toml"
    f" --deps-dir {build_dir}/deps --output-dir {pkg_dir}"
)


_imported: dict[str, tuple[ImportedTarget, Path]] = {}


def _fetched(name: str) -> tuple[ImportedTarget, Path]:
    """The imported target for a fetched package, and its unpacked source tree.

    Memoized: an ImportedTarget registers itself under the package name, so a
    second call would fail with *Target 'nanobind' already exists*.
    """
    if name in _imported:
        return _imported[name]

    description = pkg_dir / f"{name}.pcons-pkg.toml"
    if not description.exists():
        raise RuntimeError(f"{name} package not found - {FETCH_HINT}")
    package = PackageDescription.from_toml(description)
    _imported[name] = (ImportedTarget.from_package(package), Path(package.prefix))
    return _imported[name]


# =============================================================================
# Interpreter
# =============================================================================

# Everything here describes one interpreter, the one pcons is running under.
# Building against its headers and importing with another is the failure mode
# this pins down.
interpreter = sys.executable

python_include = sysconfig.get_paths()["include"]


def _require_python_headers() -> str:
    """The CPython include directory, or a message naming what is missing.

    An interpreter without its development headers is an ordinary way for a
    distribution to be packaged, and the failure is otherwise a Python.h not
    found from inside nanobind, several includes deep.
    """
    if not (Path(python_include) / "Python.h").is_file():
        raise RuntimeError(
            f"Python.h not found in {python_include} - install the development"
            f" headers for {interpreter} (python3-dev on Debian and Ubuntu)"
        )
    return python_include


# ".cpython-314-x86_64-linux-gnu.so" - an extension not carrying it is invisible
# to the import machinery.
ext_suffix = sysconfig.get_config_var("EXT_SUFFIX")


# =============================================================================
# Targets
# =============================================================================


def module_dir(project) -> Path:
    """Where a target declared in the current subdirectory lands.

    A target goes under the build directory at its source directory's relative
    path. output_nodes would say so exactly, but the resolver fills those in
    after the build script has run. Derived once here, because a stub written to
    one directory and a PYTHONPATH pointing at another is a silent mismatch.
    """
    return Path(build_dir) / project.current_dir.relative_to(project_dir)


def nanobind_library(project, env):
    """The amalgamated nanobind runtime, built once and shared by every extension.

    NB_DOMAIN is deliberately not set. It keys the type registry, so every
    translation unit that meets another has to agree on it, and there is no way
    to make a Python constant and a CMake variable agree. Under CMake the
    runtime is built by nanobind_add_module, which we do not control, so setting
    it here would put the two build systems - and the CMake extension and its
    own runtime - on different keys. The default domain is the conventional
    choice and lets a reflex extension share types with any other nanobind one.
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
    lib.public.compile_flags.append(f"-isystem{_require_python_headers()}")
    # -fPIC because the archive is linked into a shared object. Without it the
    # linker reports "failed to set dynamic section sizes: bad value", which
    # names neither the flag nor the file.
    lib.private.compile_flags.extend(
        ["-fPIC", "-fno-strict-aliasing", "-fvisibility=hidden"]
    )
    return lib


def add_stub(project, env, extension, name: str):
    """Generate a .pyi for a built extension, with nanobind's own stubgen.

    Not built by default and not checked in: the output tracks the nanobind
    version, so a golden file would churn on every bump.
    """
    _, prefix = _fetched("nanobind")
    out = module_dir(project)

    stub = project.Command(
        f"{name}-stub",
        env,
        target=out / f"{name}.pyi",
        command=[
            interpreter,
            str(prefix / "src" / "stubgen.py"),
            "-m",
            name,
            "-o",
            str(out / f"{name}.pyi"),
            "-i",
            str(out),
        ],
    )
    stub.add_dependency(extension)
    project.Alias("py-stubs", stub)
    return stub


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
