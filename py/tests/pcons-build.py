from pcons import context

from reflex_build.python import add_python_extension, add_stub
from reflex_build.testing import add_python_test, add_test

project = context.current_project
env = project.default_environment

nanobind = project.get_target("nanobind.runtime")
py = project.get_target("reflex.py")

compile_only = {"groups-limit"}
ops_limits = {"groups-limit": 16777216}

for src in project.current_dir.glob("test-*.cpp"):
    name = src.stem.removeprefix("test-")
    test = add_test(
        name,
        [src.name],
        [py],
        group="py",
        discover=None if name in compile_only else "doctest",
    )
    if name in ops_limits:
        test.private.compile_flags.append(f"-fconstexpr-ops-limit={ops_limits[name]}")
    print(f"-- Test added: {test.name}")

for src in project.current_dir.glob("ext/*.cpp"):
    name = src.stem
    extension = add_python_extension(
        project, env, name, [f"ext/{src.name}"], [nanobind]
    )
    # Headers only. An extension is a shared object and the reflex archives are
    # not built with -fPIC, so linking them is not an option; nothing in them is
    # needed anyway, the binder is entirely consteval and inline.
    extension.private.include_dirs.extend(["../include", "../../core/include"])
    test = add_python_test(
        name,
        extension,
        project.current_dir / "python" / f"test_{name}.py",
        group="py",
    )
    add_stub(project, env, extension, name)
    print(f"-- Test added: {test.name}")
