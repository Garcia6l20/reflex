from pcons import context
from reflex_build.python import add_python_extension
from reflex_build.testing import add_python_test, add_test

project = context.current_project
env = project.default_environment

nanobind = project.get_target("nanobind.runtime")
py = project.get_target("reflex.py")

for src in project.current_dir.glob("test-*.cpp"):
    test = add_test(src.stem.removeprefix("test-"), [src.name], [py], group="py")
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
    print(f"-- Test added: {test.name}")
