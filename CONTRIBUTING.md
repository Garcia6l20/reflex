# Contributing to reflex

## Requirements

- GCC >= 16.1.0. GCC 16.0.1, which Ubuntu 26.04 ships, miscompiles the
  reflection code and is not supported.
- Python >= 3.11 and [uv](https://docs.astral.sh/uv/).
- Conan, for doctest. It comes with `uv sync`.

The primary build system is [pcons](https://pypi.org/project/pcons/), which
generates Ninja files from the `pcons-build.py` script in each module. The
CMake build in the repository root exists for consumers and builds the
header-only form of every library by default. Contributions are checked against
the pcons build.

## Getting set up

```bash
uv sync --group dev
conan install . --output-folder=build/conan --build=missing
```

`reflex.py` builds a CPython extension on top of nanobind, which is fetched from
source because the wheel does not ship `tsl/robin_map.h`:

```bash
uv run python -m pcons.packages.fetch.cli fetch deps.toml \
    --deps-dir build/deps --output-dir build/pkg
```

The CPython development headers have to be installed as well, `python3-dev` on
Debian and Ubuntu.

## Building

```bash
pcons                        # release, every module
pcons --variant debug
pcons build reflex.cli       # one target
```

Options are set on the command line and persist in the build directory, the way
a CMake cache does:

```bash
pcons REFLEX_BUILD_TESTS=true
pcons cache                  # what the build directory holds
pcons --fresh                # discard it
```

Only the `VAR=value` form persists. `REFLEX_BUILD_TESTS=1 pcons` sets it for
that one run through the environment, and the next bare `pcons` silently drops
every test target.

The options are `REFLEX_BUILD_TESTS`, `REFLEX_BUILD_PROGRAMS` for the examples,
`REFLEX_COVERAGE` and `REFLEX_WERROR`, which is on by default.

## Testing

```bash
pcons REFLEX_BUILD_TESTS=true
pcons test                   # one run per doctest case
pcons test -L cli            # one group
pcons test --list            # what would run
```

A group is the module directory: `core`, `cli`, `poly`, `serde`, `jinja`, `py`.
A single binary runs on its own, and takes the usual doctest arguments:

```bash
pcons build test-cli
./build/cli/tests/reflex-test-cli-usage -tc="*usage*"
```

The Python side of `reflex.py` is tested through the interpreter:

```bash
PYTHONPATH=build/py/tests python3 py/tests/python/test_basic.py
```

## Coverage

`--coverage` touches every object file, so coverage gets its own build
directory. It has to be named twice, since `-B` moves pcons' own directory and
`PCONS_BUILD_DIR` is what the build script reads:

```bash
PCONS_BUILD_DIR=$PWD/build-cov pcons -B build-cov REFLEX_COVERAGE=true --variant debug
PCONS_BUILD_DIR=$PWD/build-cov pcons -B build-cov run coverage
```

A fresh build directory needs its own `conan install` and fetch runs first.
`REFLEX_COVERAGE` implies `REFLEX_BUILD_TESTS`.

`pcons run coverage` builds every test binary, clears the stale counters, runs
the suite and then gcovr, writing a text report, an HTML summary and a
`coverage.lcov` an editor can paint the gutter from. Useful options:

- `-f REGEX` scopes the whole report to one subtree, matched against the path
  relative to the project root.
- `--details` adds an annotated page per file. Keep it scoped with `-f`, since
  the page annotates every line of every instantiated header.
- `--fail-under RATE` exits non-zero below a line rate, for a gate.
- Anything after `--` goes to the test runner, so `-- -L cli` runs one group.

Only line coverage is worth reading. The function rate counts one function per
instantiation, and a `consteval` helper runs in the compiler and emits no
counters at all, so a file that is mostly `consteval` barely appears.

## Instantiation counts

Reflection makes it easy to instantiate a template once per type when the code
it emits does not depend on that type. `pcons run instantiations` reads the
object files and ranks what came out:

```bash
pcons run instantiations
pcons run instantiations -f '^reflex::jinja' --sort size
pcons run instantiations --bodies
```

`--bodies` compares the emitted code and ranks the templates whose body is
identical across instantiations, which is what a non-template base class or a
`std::function_ref` parameter can collapse. `--save` and `--baseline` diff two
runs.

## Repository layout

Each module is a directory of its own, with its own `pcons-build.py`:

```
<module>/modules/reflex/<name>.cppm    # C++26 module, import reflex.<name>;
<module>/include/reflex/               # header-only form of the same code
<module>/src/                          # .cpp implementations, if any
<module>/tests/test-*.cpp              # doctest tests
<module>/pcons-build.py
```

Both forms are built from the same headers, so a change belongs in
`include/reflex/` and the `.cppm` only exports it.

The build system itself lives in `reflex_build/`: `config.py` detects the
toolchain and reads the options, `testing.py` declares the test targets,
`coverage.py` and `instantiations.py` declare the two `pcons run` commands.

## C++ conventions

- Prefer `or`, `and`, `not` over `||`, `&&`, `!`.
- Always brace an `if` body, even a single line.
- Prefer reflection over macro metaprogramming, over hand-written enum and
  string tables, and over tuple boilerplate.
- Keep the change focused. Fix the root cause rather than adding a narrow
  workaround, and leave unrelated code alone.
- Follow the style of the file you are editing. `clang-format` is configured.
- Document public entities with in doxygen style.
- When behaviour changes, add or update the test nearest the code you touched.

Reflection support is young and GCC has bugs in it. When you work around one,
say which one, so the workaround can be removed later.

## Adding a test

Drop a `test-<name>.cpp` in the module's `tests/` directory. The build script
globs them, so nothing else is needed for the common case. `add_test` from
`reflex_build.testing` takes `discover=None` for a test whose assertions are
all `static_assert`s, where compiling it is the test.

Name a `TEST_CASE` after the behaviour it pins, not after the function it
calls. The name is what the runner prints when it fails.

## Commits

- No Conventional Commits prefix.
- Subject under 80 characters, leading capital, no trailing period.
- The body explains why, not what the diff already shows.
- List the changes as bullets, past tense, with a trailing period.

```
Skip an empty argv element inside a repeated argument

The main parsing loop steps over an empty element of argv, but the loop that
fills a repeated argument consumed whatever was left verbatim.

- Skipped an empty element in the repeated argument loop.
- Added tests for a repeated trailing argument.
```

## Pull requests

Branch off `devel` and target `devel`. `main` only takes merges from `devel`.

Fill the pull request template. Before pushing:

```bash
pcons REFLEX_BUILD_TESTS=true
pcons test
```

CI builds on Ubuntu with GCC 16.1.0 from Debian sid and runs the same two
commands.

## Reporting a bug

Open an issue with the bug report template. A reflection diagnostic is long,
don't paste it whole, filter them for sensitive information.
