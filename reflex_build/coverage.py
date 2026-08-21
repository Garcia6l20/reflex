import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import click
from pcons import context

from reflex_build.config import build_dir, coverage, project_dir

EXCLUDES = [
    r"(.*/)?tests/.*",
    r"(.*/)?examples/.*",
    r"(.*/)?validation/.*",
]

TRACE = re.compile(r"^\(TRACE\)")
INSTANTIATION_RECORD = re.compile(r"^(FN|BR)")


def _excludes() -> list[str]:
    patterns = list(EXCLUDES)
    try:
        inside = build_dir.resolve().relative_to(project_dir.resolve())
    except ValueError:
        return patterns
    return [*patterns, f"{re.escape(str(inside))}/.*"]


def _gcovr_argv() -> list[str]:
    program = shutil.which("gcovr")
    return [program] if program else [sys.executable, "-m", "gcovr"]


def _run_gcovr(argv: list[str]) -> int:
    process = subprocess.Popen(argv, stderr=subprocess.PIPE, text=True)
    assert process.stderr is not None
    for line in process.stderr:
        if not TRACE.match(line):
            sys.stderr.write(line)
    return process.wait()


def _strip_instantiations(info: Path) -> None:
    """Drop the per-function and per-branch records from an LCOV file.

    One template instantiation is one FN record carrying a mangled name, so the
    whole-project file is 133 MB and no editor will load it. The line records
    are 288 KB of it.
    """
    stripped = info.with_suffix(".stripped")
    with info.open() as source, stripped.open("w") as target:
        target.writelines(
            line for line in source if not INSTANTIATION_RECORD.match(line)
        )
    stripped.replace(info)


def register() -> None:
    if not coverage:
        return

    from reflex_build.testing import tests

    project = context.current_project
    report_dir = build_dir / "coverage"

    @project.cli_command("coverage")
    @click.option(
        "--filter",
        "-f",
        "filters",
        multiple=True,
        metavar="REGEX",
        help="Report only on paths matching REGEX, relative to the project root.",
    )
    @click.option("--html/--no-html", default=True, help="Write an HTML summary.")
    @click.option(
        "--details",
        is_flag=True,
        help="Add an annotated page per source file, in a directory tree.",
    )
    @click.option(
        "--lcov/--no-lcov",
        default=True,
        help="Write coverage.lcov, for an editor that shows coverage in the gutter.",
    )
    @click.option(
        "--lcov-instantiations",
        is_flag=True,
        help="Keep the per-function and per-branch LCOV records. Hundreds of megabytes.",
    )
    @click.option("--xml", is_flag=True, help="Write a Cobertura report.")
    @click.option(
        "--fail-under",
        type=float,
        default=None,
        metavar="RATE",
        help="Exit non-zero below this line rate, in percent.",
    )
    @click.option(
        "--gcov-executable",
        default="gcov",
        metavar="PROG",
        help="The gcov matching the compiler that built the objects.",
    )
    @click.option(
        "--keep-counters",
        is_flag=True,
        help="Add to the counters an earlier run left instead of resetting them.",
    )
    @click.argument("test_args", nargs=-1, type=click.UNPROCESSED)
    def coverage_report(
        filters: tuple[str, ...],
        html: bool,
        details: bool,
        lcov: bool,
        lcov_instantiations: bool,
        xml: bool,
        fail_under: float | None,
        gcov_executable: str,
        keep_counters: bool,
        test_args: tuple[str, ...],
    ) -> None:
        """Run the tests and report line coverage.

        Extra arguments go to the test runner, so `pcons run coverage -- -L core`
        covers one group. A test that never runs still reads as 0%, since its
        objects are compiled but write no counters.
        """
        from pcons.test_runner import main as test_main

        if not keep_counters:
            for counter in build_dir.rglob("*.gcda"):
                counter.unlink()

        test_status = test_main(["-B", str(build_dir), *test_args])

        if report_dir.exists():
            shutil.rmtree(report_dir)
        report_dir.mkdir(parents=True)
        info = report_dir / "coverage.lcov"
        argv = [
            *_gcovr_argv(),
            "--root",
            str(project_dir),
            str(build_dir),
            "--gcov-executable",
            gcov_executable,
            "--merge-lines",
            "--exclude-unreachable-branches",
            "-j",
            str(os.cpu_count() or 1),
            "--print-summary",
            "--txt",
            str(report_dir / "coverage.txt"),
        ]
        for pattern in _excludes():
            argv += ["--exclude", pattern]
        for pattern in filters:
            argv += ["--filter", pattern]
        if details:
            argv += ["--html-nested", str(report_dir / "index.html")]
        elif html:
            argv += ["--html", str(report_dir / "index.html")]
        if lcov:
            argv += ["--lcov", str(info)]
        if xml:
            argv += ["--cobertura", str(report_dir / "coverage.xml")]
        if fail_under is not None:
            argv += ["--fail-under-line", str(fail_under)]

        try:
            status = _run_gcovr(argv)
        except FileNotFoundError as exc:
            raise click.ClickException(
                "gcovr not found - install it with `uv sync` or `pip install gcovr`."
            ) from exc

        if lcov and not lcov_instantiations and info.exists():
            _strip_instantiations(info)

        print(f"Report written to {report_dir}")
        if status:
            raise SystemExit(status)
        if test_status:
            raise SystemExit(test_status)

    coverage_report.depends(*tests)
