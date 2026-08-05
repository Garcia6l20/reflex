from pcons import context

project = context.current_project
env = project.default_environment

convert = project.Program("reflex-serde-convert", env, sources=["convert.cpp"])
convert.private.link_libs.append(project.get_target("reflex.serde"))
convert.private.link_libs.append(project.get_target("reflex.cli"))
