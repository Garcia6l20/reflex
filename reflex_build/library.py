from pathlib import Path

from pcons import context
from pcons.core.target import Target

from reflex_build.config import build_modules


def reflex_library(
    name: str,
    *,
    module_sources: list[str],
    sources: list[str] | None = None,
    include_dir: str | Path | None = "include",
    link_libs: list[Target] | None = None,
) -> Target:
    """Declare a reflex library in whichever build mode is active.

    @p module_sources are the module interface units, compiled only when
    REFLEX_MODULES is on. @p sources are ordinary translation units, compiled in
    both modes. With modules off and no @p sources, the result is an interface
    target.

    @param name Target name, for instance `reflex.core`.
    @param module_sources Module interface units, relative to the current directory.
    @param sources Ordinary translation units, relative to the current directory.
    @param include_dir Public include directory, or None to inherit one.
    @param link_libs Targets to propagate to dependents.
    @return The declared target.
    """
    project = context.current_project
    srcs = [*module_sources, *(sources or [])] if build_modules else list(sources or [])

    if srcs:
        target = project.StaticLibrary(name, project.default_environment, sources=srcs)
        if include_dir:
            target.public.include_dirs.append(include_dir)
    else:
        target = project.HeaderOnlyLibrary(name)
        if include_dir:
            target.public.include_dirs.append(include_dir)

    if link_libs:
        target.public.link_libs.extend(link_libs)
    return target
