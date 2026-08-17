from pcons import context, write_file

from reflex_build.config import build_dir, build_testing

project = context.current_project
env = project.default_environment

convert = project.Program("reflex-serde-convert", env, sources=["convert.cpp"])
convert.private.link_libs.extend(
    project.get_targets(
        "reflex.serde",
        "reflex.serde.json",
        "reflex.serde.bson",
        "reflex.serde.yaml",
        "reflex.serde.toml",
        "reflex.cli",
    )
)

if build_testing:
    work = build_dir / "serde/programs/convert-tests"
    document = '{"a": 1, "b": "x"}\n'
    named = write_file(work / "in.json", document)
    unnamed = write_file(work / "in", document)

    for fmt in ("json", "yaml", "toml", "bson"):
        project.Test(
            f"serde.convert-inferred-{fmt}",
            convert,
            args=[str(named), str(work / f"inferred.{fmt}")],
            labels=["serde"],
        )
        # No extension on either path, so a format can only come from the flag.
        project.Test(
            f"serde.convert-forced-{fmt}",
            convert,
            args=["-if", "json", "-of", fmt, str(unnamed), str(work / f"forced-{fmt}")],
            labels=["serde"],
        )
