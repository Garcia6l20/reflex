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
| **reflex.core** | Reflection helpers, `visit`, `match`, concepts, `parse<T>`, string utilities | [core/README.md](core/README.md) |
| **reflex.cli** | Declarative command-line argument parsing + shell auto-completion | [cli/README.md](cli/README.md) |
| **reflex.poly** | Polymorphic recursive value type (`var<Ts...>`) with object/array support | [poly/README.md](poly/README.md) |
| **reflex.serde** | Reflection-driven serialization / deserialization (JSON backend included) | [serde/README.md](serde/README.md) |

---

## Taste of the API

### `reflex.cli` - annotated structs become argument parsers

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

- **GCC ≥ 16.0.1** (with C++26 `std::meta` / P2996 support)
- **CMake ≥ 4.2**

---

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

---

