# `reflex`

> **C++26 static-reflection utilities** — zero-boilerplate CLI parsing, polymorphic value types, and JSON serialization powered by `std::meta`.

---

## Overview

`reflex` leverages the C++26 static reflection API (`std::meta`, P2996) to
deliver high-level, annotation-driven utilities that eliminate boilerplate
common in modern C++ projects.

Rather than writing parsers, visitors, or serialization adapters by hand,
you annotate your structs and let the compiler derive everything at
compile time — fully type-safe, with zero overhead at runtime.

---

## Modules

| Module | Description | Docs |
|---|---|---|
| **reflex.core** | Reflection helpers, `visit`, `match`, concepts, `parse<T>`, string/constant utilities | [core/README.md](core/README.md) |
| **reflex.cli** | Declarative command-line argument parsing + shell auto-completion | [cli/README.md](cli/README.md) |
| **reflex.poly** | Polymorphic recursive value type (`var<Ts...>`) with object/array support | [poly/README.md](poly/README.md) |
| **reflex.serde** | Reflection-driven serialization / deserialization (JSON, BSON, CSV, XML backends) | [serde/README.md](serde/README.md) |
| **reflex.py** | Python bindings derived from the class declaration, on top of nanobind | [py/README.md](py/README.md) |
| **reflex.jinja** | Jinja-style templating over a reflection-derived context | [jinja/README.md](jinja/README.md) |

---

## Taste of the API

### `reflex.cli` - annotated structs and functions become argument parsers

```cpp
import reflex.cli;
using namespace reflex;

struct [[= cli::command{"Git-like tool."}]] git
{
  [[= cli::option{"-v/--verbose", "Verbosity."}.counter()]] int verbose = 0;

  struct [[= cli::command{"Push changes."}]]
  {
    [[= cli::option{"-r/--remote", "Remote name."}]] std::string remote = "origin";

    int operator()() const { std::println("pushing to {}", remote); return 0; }
  } push;
};

int main(int argc, const char** argv)
{
  return cli::run(git{}, argc, argv);
}
```

```
$ git --help
USAGE: git [OPTIONS...] ARGUMENTS...

Git-like tool.

OPTIONS:
  -h/--help        Print this message and exit.
  -v/--verbose     Verbosity.

COMMANDS:
  push             Push changes.
```

A command can also be a function, with its parameters annotated instead of a
struct's members.

```cpp
[[= cli::command{"Print a line of dots."}]]
int dots([[= cli::argument{"How many dots."}]] int count) { … }

int main(int argc, const char** argv)
{
  return cli::run<^^dots>(argc, argv);
}
```

A sub-command can be a member function, which reads the parent's options
directly. It is always a leaf: a command with sub-commands of its own stays a
nested struct.

```cpp
struct [[= cli::command{"Git-like tool."}]] git
{
  [[= cli::option{"-v/--verbose", "Verbosity."}.counter()]] int verbose = 0;

  [[= cli::command{"Push changes."}]]
  int push([[= cli::option{"-r/--remote", "Remote name."}]] std::string remote)
  {
    std::println("pushing to {} (verbose={})", remote, verbose);
    return 0;
  }
};
```

> See [cli/README.md](cli/README.md) for more details.

### `reflex.serde` - aggregate serialization with no registration

```cpp
import reflex.serde.json;

struct person { std::string name; int age = 0; };

std::ostringstream out;
reflex::serde::json::serializer{}(out, person{"Alice", 30});
// → {"name":"Alice","age":30}

auto p = reflex::serde::json::deserializer::load<person>(R"({"name":"Bob","age":25})");
// p.name == "Bob"
```

> See [serde/README.md](serde/README.md) for more details.

### `reflex.poly` - recursive variant with object/array support

```cpp
import reflex.poly;
using value = reflex::poly::var<bool, std::int64_t, double, std::string>;

value cfg = {{"host", "localhost"s}, {"port", 5432}};
cfg["host"];            // → value holding "localhost"
cfg.contains("port");   // → true
std::println("{}", cfg);// → {host:localhost,port:5432}
```

> See [poly/README.md](poly/README.md) for more details.

---

## Requirements

- **GCC ≥ 16.1.0** (with C++26 `std::meta` / P2996 support). GCC 16.0.1, as shipped by Ubuntu 26.04, miscompiles the reflection code and is not supported.
- **CMake ≥ 4.2**

---

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

`REFLEX_CXX_MODULES_ENABLED` is **OFF** by default, so this builds the header-only
form of every library and nothing is compiled as a C++20 module. CMake's module
support is not stable enough here yet. Turn it on with
`-DREFLEX_CXX_MODULES_ENABLED=ON` to get `import reflex.core;` and the rest; the
primary build system, pcons, always builds the modules.

---

