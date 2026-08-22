"""Count template instantiations in built objects, and diff two builds.

Reads the defined COMDAT symbols with nm, folds every template argument list to
`<>`, and reports how many distinct instantiations share one signature and the
code size they occupy. Only emitted code is visible this way: a fully inlined
instantiation and a consteval function leave no symbol, so read the numbers off
a -O0 build.
"""

import hashlib
import json
import re
import struct
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
    distinct: int = 0
    waste: int = 0


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


RELOCATION_WIDTH = {1: 8, 2: 4, 4: 4, 9: 4, 11: 4, 41: 4, 42: 4, 43: 4}


def demangle(names: list[str]) -> dict[str, str]:
    """Map every mangled name in @p names to its demangled spelling, in one pass."""
    if not names:
        return {}
    result = subprocess.run(
        ["c++filt"],
        input="\n".join(names),
        capture_output=True,
        text=True,
        check=False,
    )
    return dict(zip(names, result.stdout.splitlines()))


def read_object(path: Path) -> dict[str, tuple]:
    """Map each function symbol of @p path to a comparable form of its code.

    The bytes a relocation patches are masked out and replaced by the relocation
    target, so two instantiations that differ only in which specialization they
    call still compare as the same shape. objdump answers the same question, but
    it walks the symbol table once per section, which takes minutes on an object
    holding tens of thousands of COMDAT sections.
    """
    data = path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 2:
        return {}

    (shoff,) = struct.unpack_from("<Q", data, 0x28)
    shentsize, shnum, _shstrndx = struct.unpack_from("<HHH", data, 0x3A)
    headers = []
    for i in range(shnum):
        base = shoff + i * shentsize
        name, kind, _flags, _addr, offset, size, link, info, _align, entsize = (
            struct.unpack_from("<IIQQQQIIQQ", data, base)
        )
        headers.append((name, kind, offset, size, link, info, entsize))

    def name_at(table: int, offset: int) -> str:
        end = data.index(b"\0", table + offset)
        return data[table + offset : end].decode("utf-8", "replace")

    symtab = next((h for h in headers if h[1] == 2), None)
    if symtab is None:
        return {}
    strtab = headers[symtab[4]][2]

    symbols = []
    for base in range(symtab[2], symtab[2] + symtab[3], symtab[6]):
        name, info, _other, shndx, value, size = struct.unpack_from(
            "<IBBHQQ", data, base
        )
        symbols.append((name_at(strtab, name), info & 0xF, shndx, value, size))

    relocations: dict[int, list] = defaultdict(list)
    for header in headers:
        if header[1] != 4:
            continue
        for base in range(header[2], header[2] + header[3], header[6]):
            offset, info, addend = struct.unpack_from("<QQq", data, base)
            relocations[header[5]].append(
                (offset, info >> 32, info & 0xFFFFFFFF, addend)
            )

    bodies: dict[str, tuple] = {}
    for name, kind, shndx, value, size in symbols:
        if kind != 2 or not size or shndx == 0 or shndx >= len(headers):
            continue
        section = headers[shndx]
        if section[1] != 1:
            continue
        code = bytearray(data[section[2] + value : section[2] + value + size])
        patched = []
        for offset, sym, reloc, addend in relocations.get(shndx, ()):
            if not value <= offset < value + size:
                continue
            local = offset - value
            width = RELOCATION_WIDTH.get(reloc, 4)
            code[local : local + width] = b"\0" * min(width, len(code) - local)
            target = symbols[sym][0] if sym < len(symbols) else ""
            patched.append((local, reloc, target, addend))
        bodies[name] = (bytes(code), tuple(patched))
    return bodies


def read_bodies(files: list[Path]) -> dict[str, str]:
    """Map each demangled symbol to a digest of its code.

    A relocation target is folded to the signature of what it names, so an
    instantiation calling its own specialization of a helper matches one calling
    another specialization of the same helper.
    """
    raw: dict[str, tuple] = {}
    for path in files:
        for name, body in read_object(path).items():
            raw.setdefault(name, body)

    targets = {target for _, patched in raw.values() for _, _, target, _ in patched}
    spelling = demangle(sorted(set(raw) | targets))
    folded = {
        target: strip_arguments(spelling.get(target, target)) for target in targets
    }

    bodies: dict[str, str] = {}
    for name, (code, patched) in raw.items():
        shape = hashlib.blake2b(code, digest_size=16)
        for local, reloc, target, addend in patched:
            shape.update(
                f"|{local}:{reloc}:{addend}:{folded.get(target, target)}".encode()
            )
        bodies[spelling.get(name, name)] = shape.hexdigest()
    return bodies


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


def collect(
    files: list[Path], *, all_symbols: bool, by_class: bool, bodies: bool = False
) -> dict[str, Row]:
    """Group the symbols of @p files by signature, one row per template."""
    sizes = read_symbols(files, all_symbols)
    rows: dict[str, Row] = defaultdict(lambda: Row(0, 0))
    grouped: dict[str, list[str]] = defaultdict(list)
    for name, size in sizes.items():
        signature = strip_arguments(name)
        key = class_of(signature) if by_class else signature
        rows[key].count += 1
        rows[key].size += size
        grouped[key].append(name)

    if bodies:
        code = read_bodies(files)
        for key, names in grouped.items():
            seen: dict[str, int] = {}
            for name in names:
                body = code.get(name)
                if body is None:
                    continue
                seen.setdefault(body, sizes[name])
            rows[key].distinct = len(seen)
            rows[key].waste = rows[key].size - sum(seen.values())
    return dict(rows)


def load_baseline(path: Path) -> dict[str, Row]:
    raw = json.loads(path.read_text())
    return {key: Row(value["count"], value["size"]) for key, value in raw.items()}


def dump(rows: dict[str, Row]) -> str:
    return json.dumps(
        {key: {"count": row.count, "size": row.size} for key, row in rows.items()},
        indent=2,
    )


def percent(part: int, whole: int) -> float:
    return 100.0 * part / whole if whole else 0.0


def report(
    rows: dict[str, Row], top: int, floor: int, by_size: bool, bodies: bool = False
) -> None:
    selected = [(key, row) for key, row in rows.items() if row.count >= floor]
    if bodies:
        shared = [item for item in selected if item[1].distinct and item[1].waste > 0]
        shared.sort(key=lambda item: item[1].waste, reverse=True)
        print(
            f"{'count':>7} {'shapes':>7} {'bytes':>10} {'waste':>10} {'waste%':>7}"
            "  signature"
        )
        for key, row in shared[:top]:
            print(
                f"{row.count:>7} {row.distinct:>7} {row.size:>10} {row.waste:>10} "
                f"{percent(row.waste, row.size):>6.1f}%  {key}"
            )
        waste = sum(row.waste for _, row in shared)
        size = sum(row.size for _, row in shared)
        print(
            f"\n{waste} bytes in duplicate bodies over {len(shared)} signatures, "
            f"{percent(waste, size):.1f}% of the {size} they occupy"
        )
        return
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
    @click.option(
        "--bodies",
        is_flag=True,
        help="Compare the emitted code and rank the templates whose body does not "
        "depend on its type.",
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
        bodies: bool,
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

        rows = collect(
            files, all_symbols=all_symbols, by_class=group == "class", bodies=bodies
        )
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
            report(rows, limit, floor, sort == "size", bodies)
