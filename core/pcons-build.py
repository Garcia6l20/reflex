from pcons import static_library

static_library(
    "reflex.core",
    sources=[
        "modules/reflex/core.cppm",
    ],
).public.include_dirs.append("include")
