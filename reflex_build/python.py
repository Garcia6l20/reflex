import importlib.util
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
    "fetch it with:\n"
    "  python3 -m pcons.packages.fetch.cli fetch deps.toml"
    f" --deps-dir {build_dir}/deps --output-dir {pkg_dir}"
)


_imported: dict[str, tuple[ImportedTarget, Path]] = {}


def _fetched(name: str) -> tuple[ImportedTarget, Path] | None:
    """The imported target for a fetched package, and its unpacked source tree.

    None when pcons-fetch has not unpacked it into this build directory.

    Memoized: an ImportedTarget registers itself under the package name, so a
    second call would fail with *Target 'nanobind' already exists*.
    """
    if name in _imported:
        return _imported[name]

    description = pkg_dir / f"{name}.pcons-pkg.toml"
    if not description.exists():
        return None
    package = PackageDescription.from_toml(description)
    target = ImportedTarget.from_package(package)
    target.public.system_include_dirs.extend(target.public.include_dirs)
    target.public.include_dirs.clear()
    _imported[name] = (target, Path(package.prefix))
    return _imported[name]


def _is_nanobind_tree(root: Path) -> bool:
    """Whether @p root carries both the headers and the runtime source.

    An installed nanobind that ships headers alone is no use here: the runtime
    is compiled from src/nb_combined.cpp, not linked from a prebuilt library.
    """
    return (root / "include/nanobind/nanobind.h").is_file() and (
        root / "src/nb_combined.cpp"
    ).is_file()


def _importable_nanobind() -> Path | None:
    """An installed nanobind Python package, laid out the way its wheel is."""
    spec = importlib.util.find_spec("nanobind")
    if spec is None or not spec.submodule_search_locations:
        return None
    root = Path(next(iter(spec.submodule_search_locations)))
    return root if _is_nanobind_tree(root) else None


def _robin_map_include(nanobind_root: Path) -> Path | None:
    """Where tsl/robin_map.h is, next to @p nanobind_root or fetched separately.

    The release tarball's ext/robin_map is empty, since pcons-fetch clones
    without --recursive, so the fetched form always needs the separate package.
    An installed wheel may carry its own.
    """
    bundled = nanobind_root / "ext/robin_map/include"
    if (bundled / "tsl/robin_map.h").is_file():
        return bundled
    fetched = _fetched("robin-map")
    if fetched and (fetched[1] / "include/tsl/robin_map.h").is_file():
        return fetched[1] / "include"
    return None


_nanobind: tuple[Path, Path] | None = None
_nanobind_searched = False


def find_nanobind() -> tuple[Path, Path] | None:
    """nanobind's source tree and the include directory holding tsl/robin_map.h.

    Prefers what pcons-fetch unpacked into this build directory, then an
    installed nanobind package. None when neither is usable, which makes
    reflex.py skip itself: reflex is often consumed as a subdirectory by a
    project that wants nothing to do with Python bindings, and a missing
    nanobind is no reason to fail its configure.
    """
    global _nanobind, _nanobind_searched
    if _nanobind_searched:
        return _nanobind
    _nanobind_searched = True

    fetched = _fetched("nanobind")
    roots = [root for root in (fetched and fetched[1], _importable_nanobind()) if root]
    for root in roots:
        if not _is_nanobind_tree(root):
            continue
        robin_map = _robin_map_include(root)
        if robin_map:
            _nanobind = (root, robin_map)
            break
    return _nanobind


# =============================================================================
# Interpreter
# =============================================================================

# Everything here describes one interpreter, the one pcons is running under.
# Building against its headers and importing with another is the failure mode
# this pins down.
interpreter = sys.executable

python_include = sysconfig.get_paths()["include"]


def _python_headers() -> str | None:
    """The CPython include directory, or None when the headers are not installed.

    An interpreter without its development headers is an ordinary way for a
    distribution to be packaged, and the failure is otherwise a Python.h not
    found from inside nanobind, several includes deep.
    """
    if not (Path(python_include) / "Python.h").is_file():
        return None
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

    @return The runtime library, or None when no nanobind was found.
    """
    found = find_nanobind()
    if found is None:
        print(f"REFLEX_PY disabled: no nanobind found - {FETCH_HINT}")
        return None

    headers = _python_headers()
    if headers is None:
        print(
            f"REFLEX_PY disabled: Python.h not found in {python_include}"
            f" - install the development headers for {interpreter}"
            " (python3-dev on Debian and Ubuntu)"
        )
        return None

    prefix, robin_map = found

    # Not "nanobind": ImportedTarget already claims the package's own name.
    lib = project.StaticLibrary(
        "nanobind.runtime", env, sources=[prefix / "src" / "nb_combined.cpp"]
    )
    lib.public.system_include_dirs.extend([prefix / "include", robin_map])
    # A system include dir, not an include_dir: the CPython headers are not
    # ours to fix, so their warnings are silenced, and the toolchain spells it
    # the way each compiler wants (-isystem, /external:I).
    lib.public.system_include_dirs.append(headers)
    # -fPIC because the archive is linked into a shared object. Without it the
    # linker reports "failed to set dynamic section sizes: bad value", which
    # names neither the flag nor the file.
    lib.private.compile_flags.extend(
        ["-fPIC", "-fno-strict-aliasing", "-fvisibility=hidden", "-Wno-cast-qual"]
    )
    return lib


def add_stub(project, env, extension, name: str):
    """Generate a .pyi for a built extension, with nanobind's own stubgen.

    Not built by default and not checked in: the output tracks the nanobind
    version, so a golden file would churn on every bump.
    """
    prefix, _ = find_nanobind()
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
