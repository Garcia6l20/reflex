# reflex.serde

> **`ser`ialization / `de`serialization for C++26.**

`reflex.serde` lets you serialize and deserialize C++ aggregates, standard
containers, and `reflex::poly::var` values without writing any boilerplate -
field names, nesting, and type dispatch are all derived automatically via
C++26 static reflection.

Currently ships with a **JSON** backend (`reflex.serde.json`).

---

## Modules

| Module | Purpose |
|---|---|
| `reflex.serde` | Core concepts, annotations, `object_visit` |
| `reflex.serde.json` | JSON serializer + deserializer |
| `reflex.serde.bson` | BSON serializer + deserializer |

---

## Quick start

```cpp
import reflex.serde.json;
using namespace reflex::serde;

struct person
{
  std::string name;
  int         age  = 0;
  bool        active = true;
};

// Serialize
person p{"Alice", 30, true};
std::ostringstream out;
json::serializer{}(out, p);
// → {"name":"Alice","age":30,"active":true}

// Deserialize
auto p2 = json::deserializer::load<person>(R"({"name":"Bob","age":25,"active":false})");
// p2.name == "Bob", p2.age == 25
```

---

## JSON serializer

`json::serializer` is a callable that writes to any `std::ostream`-compatible
output.  It handles:

| C++ type | JSON output |
|---|---|
| `null_t` | `null` |
| `bool` | `true` / `false` |
| Integers / floats (`number_c`) | bare number |
| Strings (`str_c`) | `"…"` with escape handling |
| Sequences (`seq_c`) | `[…]` |
| Maps (`map_c`) | `{key:value,…}` |
| Aggregates (`aggregate_c`) | `{"field":value,…}` via reflection |
| `reflex::poly::var` | dispatched via `reflex::visit` |

```cpp
json::serializer ser;
std::ostringstream ss;
ser(ss, std::vector<int>{1, 2, 3}); // → [1,2,3]
ser(ss, std::map<std::string,int>{{"a",1}}); // → {"a":1}
```

---

## JSON deserializer

`json::deserializer::load<T>(string)` parses JSON text into `T`.

```cpp
// Into a json::value (poly::var)
auto v = json::deserializer::load(R"([1,"two",null])");
v.is_array();          // true
v[0] == 1;             // true

// Into a concrete struct (reflection-driven field matching)
struct config { std::string host; int port = 80; };
auto cfg = json::deserializer::load<config>(R"({"host":"localhost","port":8080})");
```

Supported JSON → C++ mappings:

| JSON | C++ |
|---|---|
| `null` | `null_t` |
| `true` / `false` | `bool` |
| Number | `int`, `double`, … (`number_c`) |
| `"string"` | `std::string` (`str_c`) |
| `[…]` | `std::vector<T>` (`seq_c`) |
| `{…}` | aggregate struct or `map_c` |

---

## Field renaming with annotations

Use `serde::name` to override the JSON key for a field:

```cpp
import reflex.serde;

struct my_struct
{
  [[= serde::name{"first_name"}]] std::string firstName;
  int age = 0;
};
// serializes as: {"first_name":"…","age":0}
```

---

## `poly` serde

`reflex.serde` also provides direct support for `reflex::poly::var`:

```cpp
import reflex.serde.json;
using value = reflex::poly::var<bool, std::int64_t, double, std::string>;

auto v = json::deserializer::load<value>(R"({"x":1,"y":2})");
v.is_object(); // true
```

---

> See [tests](tests) for more examples.
