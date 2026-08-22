"""Count template instantiations in built objects, and diff two builds.

Reads the defined COMDAT symbols with nm, folds every template argument list to
`<>`, and reports how many distinct instantiations share one signature and the
code size they occupy. Only emitted code is visible this way: a fully inlined
instantiation and a consteval function leave no symbol, so read the numbers off
a -O0 build.
"""

import json
import re
import subprocess
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

import click
from pcons import context

from reflex_build.config import build_dir

COMDAT_TYPES = frozenset("VWvw")
OPERATOR = re.compile(r"operator\s*(<=>|<<=|>>=|<<|>>|<=|>=|<|>)")
NM_LINE = re.compile(
    r"^(?P<value>\d+)\s+(?:(?P<size>\d+)\s+)?(?P<type>\S)\s+(?P<name>.+)$"
)


@dataclass
class Row:
    """One group of symbols sharing a signature."""

    count: int
    size: int


def strip_arguments(name: str) -> str:
    """Fold every template argument list in @p name to `<>`."""
    out: list[str] = []
    depth = 0
    i = 0
    while i < len(name):
        operator = OPERATOR.match(name, i)
        if operator and depth == 0:
            out.append(operator.group(0))
            i = operator.end()
            continue
        c = name[i]
        if c == "<":
            depth += 1
            if depth == 1:
                out.append("<>")
        elif c == ">":
            depth = max(depth - 1, 0)
        elif depth == 0:
            out.append(c)
        i += 1
    return "".join(out)


def without_parameters(signature: str) -> str:
    """Drop the trailing parameter list of @p signature, if it has one."""
    end = signature.rfind(")")
    if end == -1:
        return signature
    depth = 0
    for i in range(end, -1, -1):
        if signature[i] == ")":
            depth += 1
        elif signature[i] == "(":
            depth -= 1
            if depth == 0:
                return signature[:i].rstrip()
    return signature


def class_of(signature: str) -> str:
    """Reduce @p signature to the class owning it, without return type or member name."""
    head = without_parameters(signature).rsplit(" ", 1)[-1]
    return head.rsplit("::", 1)[0] if "::" in head else head


def object_files(paths: list[Path]) -> list[Path]:
    found: list[Path] = []
    for path in paths:
        if path.is_dir():
            found.extend(sorted(path.rglob("*.o")))
        else:
            found.append(path)
    return found


def read_symbols(files: list[Path], all_types: bool) -> dict[str, int]:
    """Map each distinct demangled symbol to its size, deduplicated across objects."""
    sizes: dict[str, int] = {}
    for batch in (files[i : i + 256] for i in range(0, len(files), 256)):
        result = subprocess.run(
            [
                "nm",
                "--demangle",
                "--print-size",
                "--defined-only",
                "-t",
                "d",
                *map(str, batch),
            ],
            capture_output=True,
            text=True,
            check=False,
        )
        for line in result.stdout.splitlines():
            match = NM_LINE.match(line)
            if not match:
                continue
            if not all_types and match["type"] not in COMDAT_TYPES:
                continue
            size = int(match["size"] or 0)
            name = match["name"]
            sizes[name] = max(sizes.get(name, 0), size)
    return sizes


def collect(files: list[Path], *, all_symbols: bool, by_class: bool) -> dict[str, Row]:
    """Group the symbols of @p files by signature, one row per template."""
    rows: dict[str, Row] = defaultdict(lambda: Row(0, 0))
    for name, size in read_symbols(files, all_symbols).items():
        signature = strip_arguments(name)
        key = class_of(signature) if by_class else signature
        rows[key].count += 1
        rows[key].size += size
    return dict(rows)


def load_baseline(path: Path) -> dict[str, Row]:
    raw = json.loads(path.read_text())
    return {key: Row(value["count"], value["size"]) for key, value in raw.items()}


def dump(rows: dict[str, Row]) -> str:
    return json.dumps(
        {key: {"count": row.count, "size": row.size} for key, row in rows.items()},
        indent=2,
    )


def report(rows: dict[str, Row], top: int, floor: int, by_size: bool) -> None:
    selected = [(key, row) for key, row in rows.items() if row.count >= floor]
    selected.sort(
        key=lambda item: item[1].size if by_size else item[1].count, reverse=True
    )
    print(f"{'count':>7} {'bytes':>10}  signature")
    for key, row in selected[:top]:
        print(f"{row.count:>7} {row.size:>10}  {key}")
    print(
        f"\n{sum(row.count for row in rows.values())} instantiations, "
        f"{len(rows)} signatures, {sum(row.size for row in rows.values())} bytes"
    )


def report_delta(
    rows: dict[str, Row], baseline: dict[str, Row], top: int, by_size: bool
) -> None:
    deltas = []
    for key in set(rows) | set(baseline):
        now = rows.get(key, Row(0, 0))
        was = baseline.get(key, Row(0, 0))
        if now.count != was.count or now.size != was.size:
            deltas.append((key, now.count - was.count, now.size - was.size, now.count))
    deltas.sort(key=lambda item: abs(item[2] if by_size else item[1]), reverse=True)
    print(f"{'delta':>7} {'bytes':>10} {'now':>7}  signature")
    for key, count, size, now in deltas[:top]:
        print(f"{count:>+7} {size:>+10} {now:>7}  {key}")
    before, after = (
        sum(row.count for row in source.values()) for source in (baseline, rows)
    )
    before_size, after_size = (
        sum(row.size for row in source.values()) for source in (baseline, rows)
    )
    print(
        f"\n{before} -> {after} instantiations ({after - before:+}), "
        f"{before_size} -> {after_size} bytes ({after_size - before_size:+})"
    )


def register() -> None:
    project = context.current_project

    @project.cli_command("instantiations")
    @click.option(
        "--filter",
        "-f",
        "pattern",
        metavar="REGEX",
        help="Keep only signatures matching REGEX.",
    )
    @click.option(
        "--group",
        "-g",
        type=click.Choice(["symbol", "class"]),
        default="symbol",
        help="Group by full signature, or by the class owning it.",
    )
    @click.option("--top", "-n", type=int, default=25, help="Rows to print. 0 for all.")
    @click.option(
        "--min",
        "-m",
        "floor",
        type=int,
        default=2,
        help="Skip signatures below this count.",
    )
    @click.option(
        "--sort",
        "-s",
        type=click.Choice(["count", "size"]),
        default="count",
        help="Rank by instantiation count or by bytes.",
    )
    @click.option("--all-symbols", is_flag=True, help="Count non-COMDAT symbols too.")
    @click.option(
        "--save", type=click.Path(path_type=Path), help="Write the counts as JSON."
    )
    @click.option(
        "--baseline",
        type=click.Path(exists=True, path_type=Path),
        help="Diff against counts saved earlier.",
    )
    @click.option("--json", "as_json", is_flag=True, help="Print the counts as JSON.")
    @click.argument("paths", nargs=-1, type=click.Path(exists=True, path_type=Path))
    def instantiations(
        pattern: str | None,
        group: str,
        top: int,
        floor: int,
        sort: str,
        all_symbols: bool,
        save: Path | None,
        baseline: Path | None,
        as_json: bool,
        paths: tuple[Path, ...],
    ) -> None:
        """Report the template instantiations emitted in PATHS.

        PATHS defaults to the build directory. An optimized build inlines much
        of what this counts, so prefer a debug or coverage one.
        """
        files = object_files(list(paths) or [build_dir])
        if not files:
            raise click.ClickException("No object files found.")

        rows = collect(files, all_symbols=all_symbols, by_class=group == "class")
        if pattern:
            keep = re.compile(pattern)
            rows = {key: row for key, row in rows.items() if keep.search(key)}

        if save:
            save.write_text(dump(rows))

        limit = top or len(rows) or 1
        if as_json:
            print(dump(rows))
        elif baseline:
            report_delta(rows, load_baseline(baseline), limit, sort == "size")
        else:
            report(rows, limit, floor, sort == "size")
