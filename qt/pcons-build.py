import subprocess

from pcons import add_subdirectory, context

from reflex_build.config import build_programs, build_testing, qt_allow_untested
from reflex_build.qt import add_metatypes, qml_module, use_qt

project = context.current_project
env = project.default_environment

qt = use_qt(project, env, ["Core"])

if qt is None:
    print("-- reflex.qt skipped: Qt 6 not found")
else:
    qt_lib = project.HeaderOnlyLibrary("reflex.qt", include_dirs=["include"])
    if qt_allow_untested:
        qt_lib.public.defines.append("REFLEX_QT_ALLOW_UNTESTED_QT")
    qt_lib.public.link_libs.extend(
        [
            project.get_target("reflex.core"),
            project.get_target("reflex.serde"),
            qt.Core,
        ]
    )

    if build_testing:
        add_subdirectory("tests")

    if build_programs:

        @project.cli_group()
        def qt_example():
            """Reflex Qt examples"""

        qt_widgets = use_qt(project, env, ["Widgets"])
        if qt_widgets is None:
            print("-- reflex.qt examples skipped: Qt 6 Widgets not found")
        else:
            example = project.QtProgram(
                "reflex-qt-widgets",
                env,
                sources=["examples/widgets/main.cpp"],
                link=[qt_widgets.Widgets, qt_lib],
            )
            print(f"-- Example added: {example.name}")

            qt_example.depends(example)

            @qt_example.command()
            def widgets():
                """Run the widgets example"""
                subprocess.run([str(example.output_nodes[0].path)], check=True)

        qt_qml = use_qt(project, env, ["Qml"])
        if qt_qml is None:
            print("-- reflex.qt QML examples skipped: Qt 6 Qml not found")
        else:
            from_root = project.current_dir.relative_to(project.root_dir)

            def qml_example(name, uri, *, sources, qml_files):
                here = f"examples/{name}"
                metatypes, exporter = add_metatypes(
                    f"reflex-qt-{name}",
                    [f"{here}/module.cpp"],
                    link=[qt_qml.Qml],
                    include_roots=["examples"],
                )

                types = qml_module(
                    f"reflex-qt-{name}-types",
                    env,
                    uri=uri,
                    qml_files=[f"{from_root}/{here}/{f}" for f in qml_files],
                    metatypes=metatypes,
                    link=[qt_qml.Qml, qt_lib],
                )
                types.depends(exporter)
                types.private.include_dirs.append("examples")

                app = project.QtProgram(
                    f"reflex-qt-{name}",
                    env,
                    sources=[f"{here}/{s}" for s in sources],
                    link=[qt_qml.Qml, qt_lib],
                )
                app.private.include_dirs.append("examples")
                app.link(types)
                print(f"-- Example added: {app.name}")

                qt_example.depends(app)
                return app

            clock_app = qml_example(
                "clock", "Reflex.Clock", sources=["main.cpp"], qml_files=["Main.qml"]
            )
            sandbox_app = qml_example(
                "sandbox", "Reflex.Sandbox", sources=["main.cpp"], qml_files=["Main.qml"]
            )

            @qt_example.command()
            def clock():
                """Run the QML clock example"""
                subprocess.run([str(clock_app.output_nodes[0].path)], check=True)

            @qt_example.command()
            def sandbox():
                """Run the QML sandbox example"""
                subprocess.run([str(sandbox_app.output_nodes[0].path)], check=True)
