from pcons import context

project = context.current_project
env = project.default_environment

convert = project.Program("reflex-serde-convert", env, sources=["convert.cpp"])
convert.private.link_libs.extend(
    project.get_targets(
        "reflex.serde", "reflex.serde.json", "reflex.serde.bson", "reflex.cli"
    )
)
